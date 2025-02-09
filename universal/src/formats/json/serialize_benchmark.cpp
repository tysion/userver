#include <string_view>
#include <variant>

#include <benchmark/benchmark.h>
#include <rapidjson/document.h>

#include <userver/formats/json/impl/types.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/serialize_variant.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/parse/common_containers.hpp>
#include <userver/formats/parse/variant.hpp>
#include <userver/formats/serialize/common_containers.hpp>

USERVER_NAMESPACE_BEGIN

namespace {

std::string MakeStringOfDeepObject(std::size_t depth) {
  std::string str;
  str.reserve(depth * 6 + 1);
  for (std::size_t i = 0; i < depth; ++i) {
    str += R"({"a":)";
  }
  str += "1";
  for (std::size_t i = 0; i < depth; ++i) {
    str += "}";
  }
  return str;
}

constexpr std::string_view str_small_json =
    R"({"c":1,"b":{"b":{"b":1,"a":1}},"a":1})";

constexpr std::string_view str_middle_json =
    R"({"widget": {
    "debug": "on",
    "window": {
      "title": "Sample Konfabulator Widget",
      "name": "main_window",
      "width": 500,
      "height": 500
    },
    "image": {
      "src": "Images/Sun.png",
      "name": "sun1",
      "hOffset": 250,
      "vOffset": 250,
      "alignment": "center"
    },
    "text": {
      "data": "Click Here",
      "size": 36,
      "style": "bold",
      "name": "text1",
      "hOffset": 250,
      "vOffset": 100,
      "alignment": "center",
      "onMouseUp": "sun1.opacity = (sun1.opacity / 100) * 90;"
    }
  }
})";

// json was taken from https://www.json.org/example
constexpr std::string_view str_width_json =
    R"({"web-app": {
  "servlet": [
    {
      "servlet-name": "cofaxCDS",
      "servlet-class": "org.cofax.cds.CDSServlet",
      "init-param": {
        "configGlossary:installationAt": "Philadelphia, PA",
        "configGlossary:adminEmail": "ksm@pobox.com",
        "configGlossary:poweredBy": "Cofax",
        "configGlossary:poweredByIcon": "/images/cofax.gif",
        "configGlossary:staticPath": "/content/static",
        "templateProcessorClass": "org.cofax.WysiwygTemplate",
        "templateLoaderClass": "org.cofax.FilesTemplateLoader",
        "templatePath": "templates",
        "templateOverridePath": "",
        "defaultListTemplate": "listTemplate.htm",
        "defaultFileTemplate": "articleTemplate.htm",
        "useJSP": false,
        "jspListTemplate": "listTemplate.jsp",
        "jspFileTemplate": "articleTemplate.jsp",
        "cachePackageTagsTrack": 200,
        "cachePackageTagsStore": 200,
        "cachePackageTagsRefresh": 60,
        "cacheTemplatesTrack": 100,
        "cacheTemplatesStore": 50,
        "cacheTemplatesRefresh": 15,
        "cachePagesTrack": 200,
        "cachePagesStore": 100,
        "cachePagesRefresh": 10,
        "cachePagesDirtyRead": 10,
        "searchEngineListTemplate": "forSearchEnginesList.htm",
        "searchEngineFileTemplate": "forSearchEngines.htm",
        "searchEngineRobotsDb": "WEB-INF/robots.db",
        "useDataStore": true,
        "dataStoreClass": "org.cofax.SqlDataStore",
        "redirectionClass": "org.cofax.SqlRedirection",
        "dataStoreName": "cofax",
        "dataStoreDriver": "com.microsoft.jdbc.sqlserver.SQLServerDriver",
        "dataStoreUrl": "jdbc:microsoft:sqlserver://LOCALHOST:1433;DatabaseName=goon",
        "dataStoreUser": "sa",
        "dataStorePassword": "dataStoreTestQuery",
        "dataStoreTestQuery": "SET NOCOUNT ON;select test='test';",
        "dataStoreLogFile": "/usr/local/tomcat/logs/datastore.log",
        "dataStoreInitConns": 10,
        "dataStoreMaxConns": 100,
        "dataStoreConnUsageLimit": 100,
        "dataStoreLogLevel": "debug",
        "maxUrlLength": 500
      }
    },
  {
    "servlet-name": "cofaxEmail",
    "servlet-class": "org.cofax.cds.EmailServlet",
    "init-param": {
    "mailHost": "mail1",
    "mailHostOverride": "mail2"
    }
  },
  {
    "servlet-name": "cofaxAdmin",
    "servlet-class": "org.cofax.cds.AdminServlet"
  },
  {
    "servlet-name": "fileServlet",
    "servlet-class": "org.cofax.cds.FileServlet"},
  {
    "servlet-name": "cofaxTools",
    "servlet-class": "org.cofax.cms.CofaxToolsServlet",
    "init-param": {
      "templatePath": "toolstemplates/",
      "log": 1,
      "logLocation": "/usr/local/tomcat/logs/CofaxTools.log",
      "logMaxSize": "",
      "dataLog": 1,
      "dataLogLocation": "/usr/local/tomcat/logs/dataLog.log",
      "dataLogMaxSize": "",
      "removePageCache": "/content/admin/remove?cache=pages&id=",
      "removeTemplateCache": "/content/admin/remove?cache=templates&id=",
      "fileTransferFolder": "/usr/local/tomcat/webapps/content/fileTransferFolder",
      "lookInContext": 1,
      "adminGroupID": 4,
      "betaServer": true
      }
    }
  ],
  "servlet-mapping": {
    "cofaxCDS": "/",
    "cofaxEmail": "/cofaxutil/aemail/*",
    "cofaxAdmin": "/admin/*",
    "fileServlet": "/static/*",
    "cofaxTools": "/tools/*"
  },
  "taglib": {
    "taglib-uri": "cofax.tld",
    "taglib-location": "/WEB-INF/tlds/cofax.tld"
  }
}
})";

constexpr std::size_t kDepth = 127;

const std::string str_deep_json = MakeStringOfDeepObject(kDepth);

// json was generated by json random generator from
// https://bfotool.com/random-json
constexpr std::string_view str_deep_width_json = R"({
 "iockcy": {
   "xioiow": {
     "oboidegjjixq": {
    "msgeptfjcra": true
   },
   "hvraqcqfb": {
    "lnzzyezejuz": true,
    "nundpueyda": {
     "idqsr": {
      "eudmizfiv": false,
      "peujzbhrrqtu": {
       "ezbxzfbocjn": -1141791897,
       "gwves": "b-ks2KpLovxy8QXz6ma",
       "apzifwjkbmo": [
        [
         false
        ],
        "Gs9Jtv4622VPbK"
       ],
       "yohby": true,
       "gjhkwswayaw": true
      },
      "isymgmrvar": -1856858625.3054216
     },
     "jvadlxfqwtl": false,
     "yhuwoakdjm": [
      "Ox3lwSG8ZwJF",
      {
       "yypudfepqkcv": {
        "ukzjmkfsmvg": "nZ1IpQ6b8rFLBsO",
        "cjxiuljox": "9lFCY_hokEaXyncF",
        "xovrolnw": "X-Rg3b",
        "dktpl": true,
        "lujxc": [
         false,
         [
          {
           "gzyouzeafkp": [
            {
             "htjgd": {
              "mtdwjpx": "KnYuscBg",
              "eaxsjty": -1533038477,
              "lthodoet": true
             },
             "vfhcpivt": {
              "hszxj": "SBYwxFEsk7"
             },
             "dpqworixjsln": false,
             "wezifylno": 1236328084.9025648
            },
            [
             true,
             false,
             [
              false,
              false,
              [
               1275612372.6609964,
               "cHj-Qg",
               {
                "xrccb": 1833000129.4810128
               },
               "9jItuEBLhKw1IULbJh",
               {
                "gvpoegyvj": true,
                "dbbqw": true,
                "wrzmkrilmh": [
                 "mWVhr0",
                 "u-Qpjb4o",
                 {
                  "dpiuiibgbrk": {
                   "kvbtmjnmft": -918360196.8724636
                  }
                 }
                ],
                "iqpkkvc": -1209507714,
                "njuuduutq": {
                 "whztppwt": false,
                 "kfeblepj": [
                  false,
                  -907915065,
                  -1029239458.9947516,
                  false
                 ]
                }
               }
              ],
              true,
              [
               true,
               893912112.2203817,
               "Co0cVoyhlR"
              ]
             ],
             -1261304884.803812
            ],
            {
             "unshdcjoxx": {
              "wwmvayaechg": [
               1478626791.3947651
              ]
             },
             "ibaqkjlxulj": [
              {
               "tmzhdg": [
                "ZW-LEUFriIPn8rm47Kg",
                {
                 "lphmkbieqzc": -1901779610,
                 "rxllalksx": [
                  -250908377.07672802
                 ]
                },
                [
                 {
                  "bqnzrfkgzlrj": "QAYqEh8",
                  "rjkzowjpbk": "PuQF5bw",
                  "yrskazcdyal": {
                   "magmfpgrh": [
                    "uiq94-ahippmIG",
                    "pQA9mZnv"
                   ],
                   "pqwkuy": -92682496,
                   "hyxshdibbmyv": 1665998160.4760773,
                   "mwnxeobgilzg": -1016549647
                  },
                  "qjedktoz": -66847597
                 }
                ]
               ],
               "akhqzru": "80OeSmhH9mY"
              },
              "Nu1-FaDEPuyAsa",
              {
               "unfoz": 1252636695,
               "tlchevmp": true,
               "apgcrevshag": [
                -2028925509.304178,
                false,
                {
                 "cdgbfugsdlcc": "FcwvkYaX",
                 "zfvlggqerr": -1376214311.4417727,
                 "hlxrobe": -995171684
                }
               ],
               "fhvpzcclbnp": 1153999437,
               "bbgtvy": true
              },
              -1529497952.3244975
             ],
             "yhbibwss": "0syTlvLOJ18wCLS"
            },
            false,
            1033737088
           ],
           "jvblp": false,
           "kwxrb": {
            "yhtwgouzfoh": "xQ"
           },
           "oimrkrnn": "g8Mwfs4JnfEvGmXDuyo1"
          },
          -1910946432,
          {
           "camadkh": "9B",
           "rwwjipphhen": [
            true,
            "sv",
            "Led",
            true
           ]
          },
          {
           "nkyeepujzor": [
            "-mrbH0LwKYdZvvJ3uspH",
            {
             "kygxygdl": -176036538.88466898,
             "zjpqmhafbz": [
              "BEB8",
              true,
              "blO2quCf9W"
             ],
             "daxmsguymf": false
            }
           ],
           "wdnfvjxg": -2115557962.4242938
          }
         ],
         [
          "bQ",
          {
           "vlpzdrtkfu": [
            [
             [
              [
               [
                267237593,
                true,
                [
                 "B3FG3IytOClU2Tjq",
                 {
                  "dofqeiv": [
                   [
                    1428708609,
                    false,
                    true,
                    true
                   ],
                   "KIkq4ZKP8d8",
                   [
                    false
                   ],
                   [
                    142667998,
                    false
                   ]
                  ],
                  "invtxw": "tf9cafAsU",
                  "zkxoprhpkmhz": [
                   true,
                   "lJXUatkRH3Uy8iMOckCN"
                  ],
                  "oryqhdlkgnc": "o",
                  "kgejpxjjj": [
                   {
                    "qczyopk": 1141312299,
                    "twkfxzaev": "BnsZB",
                    "bdiapzychx": "nnCl1WRgqxZ2tYj",
                    "tbuulbwaw": true,
                    "vrohtfvf": "mgcjY6RQKc"
                   },
                   "6U_lp",
                   [
                    false,
                    294100973,
                    "O",
                    false,
                    "VMMbhg81oXmou_Lm3r-0"
                   ],
                   false
                  ]
                 }
                ]
               ],
               false
              ]
             ],
             false
            ],
            true,
            [
             [
              "leQOVYHkEP",
              {
               "shfnzhn": [
                {
                 "qqomoqijkf": {
                  "ukjuydw": true,
                  "znzfc": 2007334308.983658,
                  "lgmygedgfj": [
                   "BxI2ENx49RgH2"
                  ]
                 },
                 "dweqdt": -447797309,
                 "dfsqup": 2108045435,
                 "lwidqe": {
                  "teazyvyi": [
                   [
                    888214255,
                    -1349789269.492852,
                    "f",
                    false,
                    1180811394.1653957
                   ],
                   "mr",
                   [
                    -1470276840,
                    -428806862.4218451,
                    true,
                    true,
                    false
                   ],
                   -1434606556,
                   false
                  ],
                  "mflvpttrzi": "JqKzg2sWbbVUsO",
                  "pxdgmgvfhmyh": "MEYw",
                  "kbztcbqtvt": -2140993987
                 },
                 "tvtcjze": {
                  "hbxofjr": [
                   [
                    "c-h70S7fHKK88CbMAQ",
                    973328558
                   ],
                   -1657834181,
                   {
                    "juflfd": "q",
                    "emmqhec": -1333223522.005762,
                    "dlrfptbovwf": 214839891.0121772,
                    "qbjkeb": false,
                    "zbivoey": "RL"
                   }
                  ]
                 }
                },
                [
                 "dK327P0",
                 971188634,
                 {
                  "pvfvomvimbht": false,
                  "phmtg": [
                   [
                    -434241195.5097715,
                    "4ZZf5z_",
                    false,
                    1516365770.5641866,
                    false
                   ],
                   387112754.278298,
                   [
                    false,
                    "WwvE",
                    "tfrFr0",
                    -349026612
                   ],
                   971697658,
                   [
                    "8SN2RYVt-b",
                    true,
                    false
                   ]
                  ]
                 },
                 true
                ],
                false,
                1063952789
               ],
               "eizqdbyix": {
                "atiimbmvi": false,
                "tzytdhupiey": {
                 "wxihcqho": [
                  true,
                  "qhqGI_udM"
                 ],
                 "lnehnbz": "itASB"
                },
                "imljhzkhxf": [
                 "b",
                 1729698028.186914,
                 [
                  "OAwk1zC_m0B"
                 ],
                 {
                  "qvhhkz": true,
                  "oianbne": "feo0ydz6Ct74pjvot",
                  "wqwcwqve": false,
                  "siaocjq": [
                   1674311386.404033,
                   true
                  ],
                  "rtwhavmzz": "_zTI5Ap-f7W1V"
                 }
                ]
               }
              }
             ],
             [
              "Dq",
              [
               839955509.2779655,
               {
                "ibfqsshnnrgl": {
                 "oxbipp": "mT2WWeHyBBaVq",
                 "ysabb": false,
                 "awpvfvvcqr": -442775907,
                 "lsetu": "0jlu7xk"
                },
                "yisaarh": -1825855930,
                "onugpjmunrkt": true,
                "bjzqqhjx": [
                 false,
                 "W2X",
                 [
                  true,
                  "dDo_M0dlLtujFl",
                  957658968.667461,
                  [
                   false,
                   false,
                   -1241402094,
                   {
                    "nhnsqqszt": -443253850,
                    "lclazdntwvpa": false,
                    "ryoifloafexe": true
                   },
                   1424945411
                  ],
                  189578321.7072687
                 ]
                ],
                "dptpo": -492960200.1014631
               },
               [
                false,
                [
                 false
                ],
                [
                 false,
                 -527398718
                ],
                -431713542
               ],
               true,
               [
                true
               ]
              ]
             ]
            ]
           ],
           "jkfwv": "U"
          }
         ],
         [
          {
           "avufzohwzjca": 194259077,
           "cfklyimway": -1460568187.5113845,
           "pesqymifwnt": -399448226.7588591
          },
          "r",
          true
         ]
        ]
       },
       "rlnnnp": "9ab-p9n1fd"
      }
     ]
    },
    "mtftqlvjmf": 765140126,
    "hqemcyxe": "nGerdBt7DvCFwp-R6S",
    "hactbkqonlpk": -1268008225.9190662
   },
   "jqbmzmdwtm": {
    "fpwpmlphcfot": "wGcrY7Bf3GSzucAZIL",
    "pcjek": "4PTsDzszoEKJ1xR5-OJE",
    "lmgvmzwfq": "4BsOr"
   }
  },
  "mdtzqv": {
   "ahqpf": false
  },
  "vntqxitcfe": {
   "dgvprftuh": {
    "dimdkouiqa": [
     true,
     {
      "kxksjnkn": "pw-w9llp",
      "nwjsbvup": true
     },
     {
      "kohcchkpjr": {
       "gzclwxlruxj": [
        {
         "knjqopnrlj": "iYi4"
        },
        [
         true
        ],
        -484190451.5090064,
        [
         "91e",
         {
          "rvpauj": [
           301450737,
           -1063586470.6969501,
           "S0t6"
          ],
          "galohngoe": "FVFk0lUnk4z",
          "fsfeuaimrl": [
           {
            "lepofjqfp": {
             "ppfcrdyh": -1218395301
            },
            "ochxqshq": 1478155294,
            "djvvuvazypbh": "lA",
            "lrvcyzobkz": "dLrZmWFw",
            "mzsslcte": [
             false,
             {
              "zefvbxqaden": true,
              "ulptnbhws": {
               "mujptogv": -854937707,
               "dxbagf": {
                "nlsfnq": true,
                "sndiopcjzjd": false,
                "wksievp": [
                 "NJ4aYZ8XR2"
                ],
                "mbhuh": {
                 "zpxyt": "fEDl_K990-e3P",
                 "ebmyn": [
                  false,
                  {
                   "tswmel": -1708399346,
                   "nwiagr": {
                    "mkonbqnndfvn": "zJG8Rhljri",
                    "wrjlfhithhoo": true
                   },
                   "rhglr": [
                    false,
                    true,
                    true
                   ],
                   "tgcsbd": "bTrrc4A0Wy_HbUgl8"
                  }
                 ]
                },
                "baasfveihwh": {
                 "qltipehomhf": "xHDfXT3ve",
                 "ojwuxfz": {
                  "gktaqlobqpk": -1812128901,
                  "usexxc": [
                   true,
                   "S65GOiuiDNbQ6mT4uBnw"
                  ],
                  "soahb": [
                   "Swld64tgyQ",
                   {
                    "ulicf": "NgnWk",
                    "behaydiib": "kX17C_",
                    "xboqpap": true,
                    "tpbaqbqu": true,
                    "abthuzjo": false
                   },
                   false,
                   false,
                   {
                    "vqsvpuav": false,
                    "jmntaszu": false,
                    "sacoqhunlfc": false,
                    "pwqby": -335912786
                   }
                  ],
                  "qgcvez": false,
                  "xtxbyggv": "lx5SWsXe8_1Eh6jkjr"
                 },
                 "pufrz": [
                  "6w"
                 ]
                }
               }
              }
             },
             false
            ]
           },
           {
            "tlbag": [
             "S"
            ],
            "jfbrxrnc": -1332955574,
            "hxpgxso": -1059551656.4409444,
            "ujtgluwx": {
             "vuchhlprbt": [
              -1616123109,
              {
               "ewhhnizrw": true,
               "srudqin": {
                "vlxzyl": -1222281754.52117,
                "zyzlbjy": [
                 275711019.86501265,
                 807790661.6038718,
                 -1715348587.0499334,
                 {
                  "usgivwys": "kRhgde",
                  "vqnprdzs": [
                   1195007392.2663782,
                   {
                    "upgqjntxazuv": true,
                    "gmpqtics": "n",
                    "zjqea": true
                   },
                   "V3ZdK3j",
                   [
                    "rrcBWvR",
                    "_",
                    "CmMYO",
                    false
                   ]
                  ],
                  "wwqmkvurob": true
                 }
                ],
                "otgyfruj": {
                 "atigxv": {
                  "nbnwcsciiqgq": 1143415391,
                  "msvsxhxi": "SFa9UH",
                  "gpnsvz": -1347922543.6968615,
                  "onsufyt": "UGR"
                 }
                },
                "qkxbvxqbpjtp": {
                 "wgjujx": -668556449.5182756
                }
               },
               "wayasmn": [
                1169042344.593709,
                true,
                {
                 "frynfcxh": 2079815602.0155253,
                 "bxfar": false,
                 "qwmetknd": [
                  -654492903.0063504,
                  "kJ1BIy"
                 ],
                 "pbeqh": {
                  "izwcusxiq": {
                   "cheezahioo": [
                    "xpYMajTyi-eZ_7kbHv3b",
                    418570141,
                    -1753978557,
                    1510403108,
                    "i"
                   ],
                   "hfkhzr": false
                  },
                  "ftsfiynsfql": [
                   [
                    "tO--anrJ6xUhc",
                    "9yR3ULvRghHLrxmA"
                   ],
                   {
                    "vameo": 327660433,
                    "bookhuu": "ZoMHVMdez",
                    "pacbhrz": -2052516124.6855729,
                    "afkpwlp": true
                   },
                   [
                    false
                   ],
                   "rTkXiFveSdQpL6"
                  ]
                 }
                }
               ],
               "twvvgthg": "-oe0",
               "buhskzymh": [
                1900512696.4880087,
                "HiK5jn"
               ]
              },
              -414441660.98987097,
              {
               "fzujkcbozuh": [
                true,
                [
                 [
                  {
                   "yjviua": "49hqGVqFZ",
                   "vgjnkyuism": [
                    -154899711.48694503,
                    true
                   ],
                   "iaetjytc": {
                    "gdxpsdopfteh": false,
                    "llbvkqpclrg": "YhCXk1vGvEmNpyHa26"
                   }
                  },
                  [
                   true,
                   false,
                   false,
                   [
                    -1795950902.291314,
                    false,
                    "EpWSk7RGAKYZoK3Eewag",
                    "hfS2IvDcw",
                    -1541341446.7938554
                   ]
                  ],
                  true
                 ]
                ],
                888884755.430661
               ]
              }
             ]
            },
            "cxqvviqdxb": "M"
           },
           "EcsuWYKcW80"
          ],
          "ujmun": {
           "rtknn": [
            {
             "ibasxaedg": [
              [
               true,
               1163763947.0711856,
               "W0PcAUhMhz"
              ],
              92872880,
              {
               "rsxbddjqqchn": true,
               "mvokz": -209648301.0879828,
               "bdjckugqfrc": [
                -1884289789
               ],
               "huobm": 627798411
              },
              "x"
             ],
             "kfbwx": [
              {
               "ngccq": {
                "akvnywwscl": "fCyM-MGvMEef7",
                "zwfka": {
                 "gvnevevjdj": [
                  "tNWv7bI"
                 ],
                 "ofhic": true
                }
               },
               "mkelc": 2088819697.7809591,
               "vviznrevlubf": {
                "xzawpqthyp": [
                 {
                  "agapzgaiqjk": {
                   "rzokvtrlaz": -1692026128.3879101
                  },
                  "uwzoe": "wsTgx-0wEDOKe",
                  "hutqgfme": {
                   "jcvstxii": {
                    "atlbakbyz": true
                   }
                  }
                 },
                 [
                  {
                   "nsvkh": 1196362007,
                   "ulnhmirvjec": [
                    true,
                    -1945196565.3765674,
                    495172315
                   ],
                   "slqszpuw": {
                    "qytnrenb": false,
                    "kliux": false
                   },
                   "aakkjjh": {
                    "xqzfhgjqonm": 49030852,
                    "brcoprhjiz": "O2geO_b4",
                    "lhcredgsavff": 1060240208.4273535,
                    "mpoux": -1062305258.3712199,
                    "mgsgviymhf": "4S6"
                   }
                  },
                  "0wbdfHK_Mh1nBdjmK",
                  false
                 ],
                 [
                  {
                   "wlmphqaex": [
                    "Sql5n_TXs9fpcx5ebrO",
                    false
                   ]
                  },
                  1305393414,
                  false,
                  {
                   "vzopuy": {
                    "xahkszkksmf": 197538702
                   },
                   "qlazj": "PjP7",
                   "ysqjpiowo": 605308233.5028566,
                   "nvlsanyqr": false,
                   "jcgyufzyy": "NHE05aIo2s0n0"
                  }
                 ]
                ],
                "umfxqqupmhn": "O5eoIo4H"
               },
               "cegmofg": [
                true,
                [
                 true,
                 [
                  "cy",
                  49691522,
                  1028498332
                 ],
                 "hIX6MaVaVQiBMIHQ",
                 {
                  "xswrwsduyptp": {
                   "ekbvpub": true,
                   "wzzgrahgior": "VHhr6HdwXG45BgWkB"
                  },
                  "efajymiaw": true,
                  "myfvbydid": [
                   false,
                   false,
                   false,
                   "eGuG2SAGlUVto8oQWmLe"
                  ],
                  "qxjjqumpaib": false,
                  "mjtgp": [
                   1906216372.6729167
                  ]
                 },
                 "5hfmE4o6slKxgzao"
                ],
                true,
                false
               ],
               "cspek": -531350247.79366744
              },
              [
               [
                {
                 "iwyckqi": 432977451.5942614,
                 "rfzmxxfdddj": "STFB71rS5x2Rx1",
                 "ljyqzsbx": [
                  "8-wvX5SW1Myr7n",
                  "w8qPO3c-PP2JzhOi",
                  {
                   "famqvwhwyq": {
                    "qnwslcbktw": false,
                    "whevgqei": "axqEJZ5HUD"
                   }
                  },
                  [
                   true,
                   false,
                   -538096004.1744311,
                   false
                  ],
                  [
                   "bd"
                  ]
                 ]
                },
                -1244816150.8761292,
                "ozbQFOqWz_S"
               ]
              ]
             ],
             "kvgendrjx": {
              "blqhjnzps": "wW"
             },
             "asbna": 1110913508.2268875
            }
           ]
          },
          "vxgpil": "UCq6c2D2oFdhi7Vpz"
         },
         "Usxdw2Xz4M_x"
        ]
       ],
       "gprirjbdtecm": -1954272224
      },
      "irzcyntwa": "fxEZFY8mqjl"
     }
    ]
   },
   "kvromskobbt": {
    "xhcmsdtimuyx": false,
    "imdaphibz": 2007747811.6741462,
    "fffuroj": true
   },
   "jjeuwkg": "_ip6kX51iGPHrFe",
   "ntaiiloypc": 1923358511
  },
  "nsxogz": {
   "endpndkgy": [
    {
     "magghbled": 1590486996.9265656,
     "lkzxax": -587274553,
     "stnegdsh": [
      false,
      "kNXESa8dKTS"
     ],
     "xdqwgdqyw": [
      "ju596ov",
      "28WNVhnGJtGSE",
      -1542680266
     ]
    }
   ]
  },
  "qharivpp": 1086843713
 },
 "kbvnslah": [
  false,
  {
   "ratwoaaeul": true,
   "zgufrgn": {
    "chrdoi": "P2T3O0IDjB",
    "dccimlqq": [
     "qO",
     -1529104121
    ],
    "svmpdvjug": false,
    "hnjkmcpua": [
     {
      "ylrbdsphfog": -2124868241,
      "eshprf": [
       -1964802685.222051,
       "ZQp4Vp0CFwkE",
       "qYPVb4-hU8F-UY2bCB",
       -1776771685.6485212,
       false
      ],
      "plbwhgdxtnjs": false,
      "wgmvvldwziik": {
       "avxvibwlq": false,
       "vbdkwd": {
        "mzjdgqejir": {
         "wfgennxd": -1370908918,
         "oapulyxg": [
          [
           1503644741,
           "uUIyPe22yLg1P_J"
          ],
          "D",
          [
           true,
           false,
           true
          ],
          true
         ],
         "nvgsizbdtbs": {
          "oawdmcilugaj": -1479408454.4739552
         }
        },
        "upqbnjiqklr": [
         2129385725.4015512
        ],
        "spsdzpv": -504355814.73802996,
        "jrdkpbdazyb": "Wb"
       }
      }
     },
     {
      "nrxanymvqgkk": 599417768
     }
    ]
   },
   "nxmrkfuc": "w8Rsy5",
   "padxqvvnntk": true,
   "iuukrnrn": 1859975030
  },
  true
 ],
 "ohjcrqrvnzao": [
  true,
  true
 ],
 "qhzpaleopy": "ZcUt7WIw_",
 "wsedtshndtdi": false
})";

// json with approximately 6 nodes
void SmallJson(benchmark::State& state) {
  for ([[maybe_unused]] auto _ : state) {
    auto json = formats::json::FromString(str_small_json);
    benchmark::DoNotOptimize(json);
  }
}

// json consists of 3 objects each of which consists of approximately 5 children
// nodes
void MiddleJson(benchmark::State& state) {
  for ([[maybe_unused]] auto _ : state) {
    auto json = formats::json::FromString(str_middle_json);
    benchmark::DoNotOptimize(json);
  }
}

// json consists of one object of 40 nodes and several objects each of which
// consists of approximately 7 children nodes
void WidthJson(benchmark::State& state) {
  for ([[maybe_unused]] auto _ : state) {
    auto json = formats::json::FromString(str_width_json);
    benchmark::DoNotOptimize(json);
  }
}

// json consists of 500 levels each of which is a key and a value
void DeepJson(benchmark::State& state) {
  for ([[maybe_unused]] auto _ : state) {
    auto json = formats::json::FromString(str_deep_json);
    benchmark::DoNotOptimize(json);
  }
}

// json consists of 800 nodes and approximately 9 depth levels
void DeepWidthJson(benchmark::State& state) {
  for ([[maybe_unused]] auto _ : state) {
    auto json = formats::json::FromString(str_deep_width_json);
    benchmark::DoNotOptimize(json);
  }
}

BENCHMARK(SmallJson);

BENCHMARK(MiddleJson);

BENCHMARK(WidthJson);

BENCHMARK(DeepJson);

BENCHMARK(DeepWidthJson);

namespace {

struct InnerObject final {
  std::variant<int, bool, std::vector<std::string>, std::string> value;
};

formats::json::Value Serialize(const InnerObject& value,
                               formats::serialize::To<formats::json::Value>) {
  formats::json::ValueBuilder builder{};
  builder["value"] = value.value;
  return builder.ExtractValue();
}

InnerObject Parse(const formats::json::Value& value,
                  formats::parse::To<InnerObject>) {
  return {value["value"].As<decltype(InnerObject::value)>()};
}

struct OuterObject final {
  InnerObject obj;
};

formats::json::Value Serialize(const OuterObject& value,
                               formats::serialize::To<formats::json::Value>) {
  formats::json::ValueBuilder builder{};
  builder["obj"] = value.obj;
  return builder.ExtractValue();
}

OuterObject Parse(const formats::json::Value& value,
                  formats::parse::To<OuterObject>) {
  return {value["obj"].As<InnerObject>()};
}

struct PairOfArrays final {
  using Array = std::vector<OuterObject>;

  Array first;
  Array second;
};

formats::json::Value Serialize(const PairOfArrays& value,
                               formats::serialize::To<formats::json::Value>) {
  formats::json::ValueBuilder builder{};
  builder["first"] = value.first;
  builder["second"] = value.second;
  return builder.ExtractValue();
}

PairOfArrays Parse(const formats::json::Value& value,
                   formats::parse::To<PairOfArrays>) {
  return {value["first"].As<PairOfArrays::Array>(),
          value["second"].As<PairOfArrays::Array>()};
}

}  // namespace

void JsonArrayToVariantParseBenchmark(benchmark::State& state) {
  const std::size_t data_size = state.range(0);

  PairOfArrays::Array array{};
  array.reserve(data_size);
  for (std::size_t i = 0; i < data_size; ++i) {
    array.push_back(OuterObject{InnerObject{"some_string"}});
  }
  const PairOfArrays data{array, array};
  const auto json_data = formats::json::ValueBuilder{data}.ExtractValue();

  for ([[maybe_unused]] auto _ : state) {
    benchmark::DoNotOptimize(json_data.As<PairOfArrays>());
  }
}
BENCHMARK(JsonArrayToVariantParseBenchmark)->Range(16, 4096);

}  // namespace

USERVER_NAMESPACE_END
