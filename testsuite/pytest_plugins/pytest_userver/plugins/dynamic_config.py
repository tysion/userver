"""
Supply dynamic configs for the service in testsuite.
"""

import contextlib
import copy
import dataclasses
import datetime
import json
import pathlib
import typing

import pytest

from pytest_userver.plugins import caches

USERVER_CONFIG_HOOKS = [
    'userver_config_dynconf_cache',
    'userver_config_dynconf_fallback',
    'userver_config_dynconf_url',
]

_CONFIG_CACHES = ['dynamic-config-client-updater']


class BaseError(Exception):
    """Base class for exceptions from this module"""


class DynamicConfigNotFoundError(BaseError):
    """Config parameter was not found and no default was provided"""


ConfigDict = typing.Dict[str, typing.Any]


class RemoveKey:
    pass


class Missing:
    pass


@dataclasses.dataclass
class _ChangelogEntry:
    timestamp: str
    dirty_keys: typing.Set[str]
    state: ConfigDict

    @classmethod
    def new(
            cls,
            *,
            previous: typing.Optional['_ChangelogEntry'],
            timestamp: str,
    ):
        if previous:
            state = previous.state.copy()
        else:
            state = {}
        return cls(timestamp=timestamp, dirty_keys=set(), state=state)

    @property
    def has_changes(self) -> bool:
        return bool(self.dirty_keys)

    def update(self, values: ConfigDict):
        self.state.update(values)
        self.dirty_keys.update(values.keys())


@dataclasses.dataclass(frozen=True)
class Updates:
    timestamp: str
    values: ConfigDict
    removed: typing.List[str]


class _Changelog:
    entry: _ChangelogEntry
    entries: typing.List[_ChangelogEntry]

    def __init__(self):
        self.timestamp = datetime.datetime.utcfromtimestamp(0)
        self.last_entry = _ChangelogEntry.new(
            timestamp=self.service_timestamp(), previous=None,
        )
        self.entries = [self.last_entry]

    def service_timestamp(self) -> str:
        return self.timestamp.strftime('%Y-%m-%dT%H:%M:%SZ')

    def next_timestamp(self) -> str:
        self.timestamp += datetime.timedelta(seconds=1)
        return self.service_timestamp()

    def tick(self):
        self.last_entry = _ChangelogEntry.new(
            timestamp=self.next_timestamp(), previous=self.last_entry,
        )
        self.entries.append(self.last_entry)

    def get_updated_since(
            self,
            values: ConfigDict,
            updated_since: str,
            ids: typing.Optional[typing.List[str]] = None,
    ) -> Updates:
        entry = self.last_entry
        values, removed = self._get_updated_since(values, updated_since)
        self.tick()
        if ids:
            values = {name: values[name] for name in ids if name in values}
            removed = [name for name in removed if name in ids]
        return Updates(
            timestamp=entry.timestamp, values=values, removed=removed,
        )

    def _get_updated_since(
            self, values: ConfigDict, updated_since: str,
    ) -> typing.Tuple[ConfigDict, typing.List[str]]:
        if not updated_since:
            return values, []
        dirty_keys = set()
        last_known_state = {}
        for entry in reversed(self.entries):
            if entry.timestamp > updated_since:
                dirty_keys.update(entry.dirty_keys)
            else:
                last_known_state = entry.state
                break
        # We don't want to send them again
        result = {}
        removed = []
        for key in dirty_keys:
            value = values.get(key, RemoveKey)
            if last_known_state.get(key, Missing) != value:
                if value is RemoveKey:
                    removed.append(key)
                else:
                    result[key] = value
        return result, removed

    def add_entries(self, values: ConfigDict):
        self.last_entry.update(values)

    @contextlib.contextmanager
    def rollback(self, defaults: ConfigDict):
        index = len(self.entries) - 1
        try:
            yield
        finally:
            self._do_rollback(index, defaults)

    def _do_rollback(self, first_index: int, defaults: ConfigDict):
        dirty_keys = set()
        for index in range(first_index, len(self.entries)):
            dirty_keys.update(self.entries[index].dirty_keys)
        if self.last_entry.has_changes:
            self.tick()

        changes = {key: defaults.get(key, RemoveKey) for key in dirty_keys}
        self.add_entries(changes)
        self.entries = self.entries[-2:]


class DynamicConfig:
    """Simple dynamic config backend."""

    def __init__(
            self,
            *,
            initial_values: ConfigDict,
            config_cache_components: typing.Iterable[str],
            cache_invalidation_state: caches.InvalidationState,
            changelog: _Changelog,
    ):
        self._values = initial_values.copy()
        self._cache_invalidation_state = cache_invalidation_state
        self._config_cache_components = config_cache_components
        self._changelog = changelog

    def set_values(self, values: ConfigDict):
        self.set_values_unsafe(copy.deepcopy(values))

    def set_values_unsafe(self, values: ConfigDict):
        self._values.update(values)
        self._changelog.add_entries(values)
        self._sync_with_service()

    def set(self, **values):
        self.set_values(values)

    def get_values_unsafe(self) -> ConfigDict:
        return self._values

    def get(self, key: str, default: typing.Any = None) -> typing.Any:
        if key not in self._values:
            if default is not None:
                return default
            raise DynamicConfigNotFoundError(f'Config {key!r} is not found')
        return copy.deepcopy(self._values[key])

    def remove_values(self, keys):
        extra_keys = set(keys).difference(self._values.keys())
        if extra_keys:
            raise DynamicConfigNotFoundError(
                f'Attempting to remove nonexistent configs: {extra_keys}',
            )
        for key in keys:
            self._values.pop(key)
        self._changelog.add_entries({key: RemoveKey for key in keys})
        self._sync_with_service()

    def remove(self, key):
        return self.remove_values([key])

    @contextlib.contextmanager
    def modify(self, key: str) -> typing.Any:
        value = self.get(key)
        yield value
        self.set_values({key: value})

    @contextlib.contextmanager
    def modify_many(
            self, *keys: typing.Tuple[str, ...],
    ) -> typing.Tuple[typing.Any, ...]:
        values = tuple(self.get(key) for key in keys)
        yield values
        self.set_values(dict(zip(keys, values)))

    def _sync_with_service(self):
        self._cache_invalidation_state.invalidate(
            self._config_cache_components,
        )


def pytest_userver_caches_setup(userver_cache_config):
    userver_cache_config.register_incremental_cache(*_CONFIG_CACHES)


@pytest.fixture(name='dynamic_config')
def _dynamic_config(
        request,
        search_path,
        object_substitute,
        cache_invalidation_state,
        _config_service_defaults_updated,
        dynamic_config_changelog,
        _dynconfig_load_json_cached,
        dynconf_cache_names,
) -> DynamicConfig:
    """
    Fixture that allows to control dynamic config values used by the service.

    After change to the config, be sure to call:
    @code
    await service_client.update_server_state()
    @endcode

    HTTP client requests call it automatically before each request.

    @ingroup userver_testsuite_fixtures
    """
    config = DynamicConfig(
        initial_values=_config_service_defaults_updated.snapshot,
        config_cache_components=dynconf_cache_names,
        cache_invalidation_state=cache_invalidation_state,
        changelog=dynamic_config_changelog,
    )
    updates = {}
    with dynamic_config_changelog.rollback(
            _config_service_defaults_updated.snapshot,
    ):
        for path in reversed(list(search_path('config.json'))):
            values = _dynconfig_load_json_cached(path)
            updates.update(values)
        for marker in request.node.iter_markers('config'):
            marker_json = object_substitute(marker.kwargs)
            updates.update(marker_json)
        config.set_values_unsafe(updates)
        yield config


@pytest.fixture(scope='session', name='dynconf_cache_names')
def _dynconf_cache_names():
    return tuple(_CONFIG_CACHES)


@pytest.fixture(scope='session', name='_dynconfig_json_cache')
def _dynconfig_json_cache():
    return {}


@pytest.fixture(name='_dynconfig_load_json_cached')
def _dynconfig_load_json_cached(json_loads, _dynconfig_json_cache):
    def load(path: pathlib.Path):
        if path not in _dynconfig_json_cache:
            _dynconfig_json_cache[path] = json_loads(path.read_text())
        return _dynconfig_json_cache[path]

    return load


@pytest.fixture
def taxi_config(dynamic_config) -> DynamicConfig:
    """
    Deprecated, use `dynamic_config` instead.
    """
    return dynamic_config


@pytest.fixture(name='dynamic_config_fallback_patch', scope='session')
def _dynamic_config_fallback_patch() -> ConfigDict:
    """
    Override this fixture to replace some dynamic config values specifically
    for testsuite tests:

    @code
    @pytest.fixture(scope='session')
    def dynamic_config_fallback_patch():
        return {"MY_CONFIG_NAME": 42}
    @endcode

    @ingroup userver_testsuite_fixtures
    """
    return {}


@pytest.fixture(name='config_service_defaults', scope='session')
def _config_service_defaults(
        config_fallback_path, dynamic_config_fallback_patch,
) -> ConfigDict:
    """
    Fixture that returns default values for dynamic config. You may override
    it in your local conftest.py or fixture:

    @code
    @pytest.fixture(scope='session')
    def config_service_defaults():
        with open('defaults.json') as fp:
            return json.load(fp)
    @endcode

    @ingroup userver_testsuite_fixtures
    """
    if not config_fallback_path:
        return dynamic_config_fallback_patch

    if pathlib.Path(config_fallback_path).exists():
        with open(config_fallback_path, 'r', encoding='utf-8') as file:
            fallback = json.load(file)
        fallback.update(dynamic_config_fallback_patch)
        return fallback

    raise RuntimeError(
        'Either provide the path to dynamic config defaults file using '
        '--config-fallback pytest option, or override '
        f'{_config_service_defaults.__name__} fixture to provide custom '
        'dynamic config loading behavior.',
    )


@dataclasses.dataclass(frozen=False)
class _ConfigDefaults:
    snapshot: ConfigDict

    async def update(self, client, dynamic_config) -> None:
        if not self.snapshot:
            values = await client.get_dynamic_config_defaults()
            # There may already be some config overrides from the current test.
            values.update(dynamic_config.get_values_unsafe())
            self.snapshot = values
            dynamic_config.set_values(self.snapshot)


# If there is no config_fallback_path, then we want to ask the service
# for the dynamic config defaults after it's launched. It's enough to update
# defaults once per service launch.
@pytest.fixture(scope='package')
def _config_service_defaults_updated(config_service_defaults):
    return _ConfigDefaults(snapshot=config_service_defaults)


@pytest.fixture(scope='session')
def userver_config_dynconf_cache(service_tmpdir):
    def patch_config(config, _config_vars) -> None:
        components = config['components_manager']['components']
        dynamic_config_component = components.get('dynamic-config', None) or {}
        if dynamic_config_component.get('fs-cache-path', '') == '':
            return

        cache_path = service_tmpdir / 'configs' / 'config_cache.json'

        if cache_path.is_file():
            # To avoid leaking dynamic config values between test sessions
            cache_path.unlink()

        dynamic_config_component['fs-cache-path'] = str(cache_path)

    return patch_config


@pytest.fixture(scope='session')
def userver_config_dynconf_fallback(config_service_defaults):
    """
    Returns a function that adjusts the static configuration file for
    the testsuite.
    Sets `dynamic-config.defaults-path` according to `config_service_defaults`.
    Updates `dynamic-config.defaults` with `config_service_defaults`.

    @ingroup userver_testsuite_fixtures
    """

    def extract_defaults_dict(component_config, config_vars) -> dict:
        defaults_field = component_config.get('defaults', None) or {}
        if isinstance(defaults_field, dict):
            return defaults_field
        elif isinstance(defaults_field, str):
            if defaults_field.startswith('$'):
                return config_vars.get(defaults_field[1:], {})
        assert False, (
            f'Unexpected static config option '
            f'`dynamic-config.defaults`: {defaults_field!r}'
        )

    def _patch_config(config_yaml, config_vars):
        components = config_yaml['components_manager']['components']
        if components.get('dynamic-config', None) is None:
            components['dynamic-config'] = {}
        dynconf_component = components['dynamic-config']

        dynconf_component.pop('defaults-path', None)
        defaults = dict(
            extract_defaults_dict(dynconf_component, config_vars),
            **config_service_defaults,
        )
        dynconf_component['defaults'] = defaults

    return _patch_config


@pytest.fixture(scope='session')
def userver_config_dynconf_url(mockserver_info):
    """
    Returns a function that adjusts the static configuration file for
    the testsuite.
    Sets the `dynamic-config-client.config-url` to the value of mockserver
    configs-service, so that the
    @ref pytest_userver.plugins.dynamic_config.mock_configs_service
    "mock_configs_service" fixture could work.

    @ingroup userver_testsuite_fixtures
    """

    def _patch_config(config, _config_vars) -> None:
        components = config['components_manager']['components']
        client = components.get('dynamic-config-client', None)
        if client:
            client['config-url'] = mockserver_info.url('configs-service')
            client['append-path-to-url'] = True

    return _patch_config


@pytest.fixture(name='dynamic_config_changelog', scope='session')
def _dynamic_config_changelog():
    return _Changelog()


@pytest.fixture
def mock_configs_service(
        mockserver,
        dynamic_config: DynamicConfig,
        dynamic_config_changelog: _Changelog,
) -> None:
    """
    Adds a mockserver handler that forwards dynamic_config to service's
    `dynamic-config-client` component.

    @ingroup userver_testsuite_fixtures
    """

    @mockserver.json_handler('/configs-service/configs/values')
    def _mock_configs(request):
        updates = dynamic_config_changelog.get_updated_since(
            dynamic_config.get_values_unsafe(),
            request.json.get('updated_since', ''),
            request.json.get('ids'),
        )
        response = {'configs': updates.values, 'updated_at': updates.timestamp}
        if updates.removed:
            response['removed'] = updates.removed
        return response

    @mockserver.json_handler('/configs-service/configs/status')
    def _mock_configs_status(_request):
        return {'updated_at': dynamic_config_changelog.timestamp}
