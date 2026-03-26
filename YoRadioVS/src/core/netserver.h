#ifndef netserver_h
#define netserver_h
#include "../AsyncWebServer/ESPAsyncWebServer.h"
#define APPEND_GROUP(name) strcat(nsBuf, "\"" name "\",")

enum requestType_e : uint8_t  { PLAYLIST=1, STATION=2, STATIONNAME=3, ITEM=4, TITLE=5, VOLUME=6, NRSSI=7, BITRATE=8, MODE=9, EQUALIZER=10, BALANCE=11, PLAYLISTSAVED=12, STARTUP=13, GETINDEX=14, GETACTIVE=15, GETSYSTEM=16, GETSCREEN=17, GETTIMEZONE=18, GETWEATHER=19, GETCONTROLS=20, DSPON=21, SDPOS=22, SDLEN=23, SDSNUFFLE=24, SDINIT=25, GETPLAYERMODE=26, CHANGEMODE=27 };
enum import_e      : uint8_t  { IMDONE=0, IMPL=1, IMWIFI=2 };
const char emptyfs_html[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1,minimum-scale=0.25"><meta charset="UTF-8">
<link rel="icon" type="image/png" href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAMAAACdt4HsAAAAYFBMVEUAAADYw1PcyVjYxFTaxlXYxFTbx1bcyVjZxVXbyFfcyFfaxlbax1bcyVjcyVjbyFfbyFfZxVXaxlbbx1fcyFjcyVjbx1fZxVXcyFjcyVjax1bbyFfcyVjbyFfax1bWwVKMlHGzAAAAH3RSTlMA+wv0zu6dBeVqSryjMRaCU97Fjz8liNk5HbFdDnWsEHoUsAAAAeFJREFUWMPtlllyrDAMRS1P2NjMQzc9RPvf5Ut1IPYjDRbJR1KVnD8Z7i1ZsgXsh1JW3usrC9Ta+2og620DiCjaaY65U4AIpqLqBb7R3B5xJucYRpI+U7jgHwsVLgjSLu74DmSvMTdhQVMMHAYeBhiQFAO5Y3CiGFzWBhDilmKQ4zsqm5uwQGvkCRfsytFkJIOhWWo+vz8uCfWMRqEVAJwsn+PsKgFA+YJR4UWe50Oc1Gt8vrFfyGC19153+afUvVMA+ADAaH5QXhvA/wB3yEICfgAqsvys8BngiPor4AaSpM8BN7lQRrrAbcBSLvMeKqmvVhtYh8mxqjCi7Tnnk4YDKYzRy9DPA2Uy9CoYDBShsCrKitxCnUUnm7qHFwyUYTlOAXYHWxP0TTzBbm1UBGIPfMkDZRcMur1bFPdAxEQPXhI1TNLSj+HxK9l9u8H41RrcKQZub5THbdxA7M3WAZL/EvRp0PDPGEgM9CxBqo9mYMcpAAPyzNZMx2aysUUWzYSi7lzSwALGGG3rvO/zurajM4BQJh0aXAGglACYg2v6uw64h2ZJfOIcp2lxh4ZgkEncRjAKF8AtYCI53M2mQc1IlNrAM7lyZ0akHKURsVaokxuLYxfD6ot8w+nOFuyP5/wDsZKME0E1GogAAAAASUVORK5CYII=">
<title>ёRadio - WEB Board Uploader</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%;overflow-x:hidden}
body{background:#0a0a0f;color:#e3d25f;font-size:18px;font-family:system-ui,sans-serif;display:flex;flex-direction:column;min-height:100vh;position:relative}
canvas{position:fixed;top:0;left:0;width:100%;height:100%;z-index:0;pointer-events:none}
section{position:relative;z-index:1;max-width:480px;margin:auto;padding:28px 32px;background:rgba(10,10,20,.82);border:1px solid rgba(227,210,95,.18);border-radius:16px;backdrop-filter:blur(12px);box-shadow:0 0 40px rgba(227,210,95,.07),0 8px 32px rgba(0,0,0,.6)}
h2{text-align:center;font-size:22px;letter-spacing:1px;margin-bottom:18px;text-shadow:0 0 18px rgba(227,210,95,.5)}
hr{border:0;border-top:1px solid rgba(227,210,95,.15);margin:16px 0}
p{text-align:center;margin-bottom:8px}
span{color:#aaa;font-size:14px;display:block;text-align:center;line-height:1.5}
.hidden{display:none}
label{color:rgba(227,210,95,.7);font-size:14px;margin-right:6px}
a{color:#e3d25f;text-decoration:none;font-weight:bold}
a:hover{text-shadow:0 0 8px #e3d25f}
#copy{position:relative;z-index:1;text-align:center;padding:14px;font-size:13px;color:#555}
input[type=file]{color:#aaa;font-size:14px}
input[type=file]::file-selector-button,input[type=submit]{border:1px solid rgba(227,210,95,.6);color:#0a0a0f;padding:6px 18px;border-radius:20px;background:linear-gradient(135deg,#e3d25f,#c9b93f);font-size:14px;cursor:pointer;font-weight:600;transition:box-shadow .2s}
input[type=file]::file-selector-button:hover,input[type=submit]:hover{box-shadow:0 0 14px rgba(227,210,95,.5)}
input[type=submit]{font-size:15px;text-transform:uppercase;padding:9px 28px;margin-top:8px;letter-spacing:1px}
.flex{display:flex;justify-content:space-around;gap:12px;margin-top:8px;flex-wrap:wrap}
input[type=text],input[type=password]{width:160px;background:rgba(255,255,255,.05);color:#e3d25f;padding:6px 10px;font-size:15px;border:1px solid rgba(227,210,95,.25);border-radius:6px;outline:none;transition:border-color .2s}
input[type=text]:focus,input[type=password]:focus{border-color:rgba(227,210,95,.7);box-shadow:0 0 8px rgba(227,210,95,.2)}
.wrap{display:flex;flex-direction:column;align-items:center;flex:1;padding:24px 0}
</style>
<script type="text/javascript" src="/variables.js"></script>
</head>
<body>
<canvas id="yobg" style="position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  z-index: 0;
  pointer-events: none;"></canvas>
<div class="wrap">
<section>
<h2>&#1105;Radio &mdash; Web Uploader</h2>
<hr>
<span>Select <u>ALL</u> files from <i>yoRadio/data/www/</i> and upload below</span>
<hr>
<form action="/webboard" method="post" enctype="multipart/form-data">
<p><label for="www">www files:</label><input type="file" name="www" id="www" multiple></p>
<hr>
<span>&#8212; OPTIONAL &#8212;<br>Upload <i>playlist.csv</i> and <i>wifi.csv</i> from backup</span>
<p style="margin-top:10px"><label for="data">wifi/playlist:</label><input type="file" name="data" id="data" multiple></p>
<hr>
<p><input type="submit" name="submit" value="Upload Files"></p>
</form>
<div id="wupload" style="margin-top:10px">
<hr>
<form name="wifiform" method="post" enctype="multipart/form-data">
<span>&#8212; OPTIONAL &#8212;<br>Can't reach 192.168.4.1? Set WiFi credentials first</span>
<div class="flex" style="margin-top:12px">
<div><label for="ssid">SSID</label><input type="text" id="ssid" name="ssid" maxlength="30" autocomplete="off"></div>
<div><label for="pass">Pass</label><input type="password" id="pass" name="pass" maxlength="40" autocomplete="off"></div>
</div>
<p style="margin-top:10px"><input type="submit" name="submit" value="Save Credentials"></p>
</form>
</div>
</section>
<p style="position:relative;z-index:1;margin-top:14px"><a href="/emergency">emergency firmware uploader</a></p>
</div>
<div id="copy">powered by <a target="_blank" href="https://github.com/e2002/yoradio/">&#1105;Radio</a><span id="version"></span></div>
<script>
document.wifiform.action='/'+formAction;
if(playMode=='player')document.getElementById('wupload').classList.add('hidden');
document.getElementById('version').innerHTML=' | v'+yoVersion;
</script>
<script type="text/javascript" src="/bg.js?v=1.2.3"></script>
</body></html>
)rawhtml";
const char index_html[] PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <meta name="theme-color" content="#e3d25f">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="default">
  <link rel="icon" type="image/png" href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAMAAACdt4HsAAAAYFBMVEUAAADYw1PcyVjYxFTaxlXYxFTbx1bcyVjZxVXbyFfcyFfaxlbax1bcyVjcyVjbyFfbyHfZxVXaxlbbx1fcyFjcyVjbx1fZxVXcyFjcyVjax1bbyFfcyVjbyFfax1bWwVKMlHGzAAAAH3RSTlMA+wv0zu6dBeVqSryjMRaCU97Fjz8liNk5HbFdDnWsEHoUsAAAAeFJREFUWMPtlllyrDAMRS1P2NjMQzc9RPvf5Ut1IPYjDRbJR1KVnD8Z7i1ZsgXsh1JW3usrC9Ta+2og620DiCjaaY65U4AIpqLqBb7R3B5xJucYRpI+U7jgHwsVLgjSLu74DmSvMTdhQVMMHAYeBhiQFAO5Y3CiGFzWBhDilmKQ4zsqm5uwQGvkCRfsytFkJIOhWWo+vz8uCfWMRqEVAJwsn+PsKgFA+YJR4UWe50Oc1Gt8vrFfyGC19153+afUvVMA+ADAaH5QXhvA/wB3yEICfgAqsvys8BngiPor4AaSpM8BN7lQRrrAbcBSLvMeKqmvVhtYh8mxqjCi7Tnnk4YDKYzRy9DPA2Uy9CoYDBShsCrKitxCnUUnm7qHFwyUYTlOAXYHWxP0TTzBbm1UBGIPfMkDZRcMur1bFPdAxEQPXhI1TNLSj+HxK9l9u8H41RrcKQZub5THbdxA7M3WAZL/EvRp0PDPGEgM9CxBqo9mYMcpAAPyzNZMx2aysUUWzYSi7lzSwALGGG3rvO/zurajM4BQJh0aXAGglACYg2v6uw64h2ZJfOIcp2lxh4ZgkEncRjAKF8AtYCI53M2mQc1IlNrAM7lyZ0akHKURsVaokxuLYxfD6ot8w+nOFuyP5/wDsZKME0E1GogAAAAASUVORK5CYII=">
  <link rel="stylesheet" href="theme.css" type="text/css" />
  <link rel="stylesheet" href="style.css" type="text/css" />
  <script type="text/javascript" src="variables.js"></script>
  <script type="text/javascript" src="script.js"></script>
  <script type="text/javascript" src="dragpl.js"></script>
</head>
<body>
<canvas id="yobg" style="position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  z-index: 0;
  pointer-events: none;"></canvas>
<div id="content" class="hidden progmem"></div>
<div id="progress"><span id="loader"></span></div>
<div id="heap"></div>
<script type="text/javascript" src="bg.js"></script>
</body>
</html>
)";
const char emergency_form[] PROGMEM = R"(
<form method="POST" action="/update" enctype="multipart/form-data">
  <input type="hidden" name="updatetarget" value="fw" />
  <label for="uploadfile">upload firmware</label>
  <input type="file" id="uploadfile" accept=".bin,.hex" name="update" />
  <input type="submit" value="Update" />
</form>
)";
struct nsRequestParams_t
{
  requestType_e type;
  uint8_t clientId;
};

class NetServer {
  public:
    import_e importRequest;
    bool resumePlay;
    char chunkedPathBuffer[40];
    char nsBuf[BUFLEN], nsBuf2[BUFLEN];
  public:
    NetServer() {};
    bool begin(bool quiet=false);
    void loop();
    void requestOnChange(requestType_e request, uint8_t clientId);
    void setRSSI(int val) { rssi = val; };
    int  getRSSI()        { return rssi; };
    void chunkedHtmlPage(const String& contentType, AsyncWebServerRequest *request, const char * path);
    void onWsMessage(void *arg, uint8_t *data, size_t len, uint8_t clientId);
    bool irRecordEnable;
#if IR_PIN!=255
    void irToWs(const char* protocol, uint64_t irvalue);
    void irValsToWs(); 
#endif
    void resetQueue();
  private:
    requestType_e request;
    QueueHandle_t nsQueue;
    char _wscmd[65], _wsval[65];
    char wsBuf[BUFLEN*2];
    int rssi;
    uint32_t playerBufMax;
    void getPlaylist(uint8_t clientId);
    bool importPlaylist();
    static size_t chunkedHtmlPageCallback(uint8_t* buffer, size_t maxLen, size_t index);
    void processQueue();
    int _readPlaylistLine(File &file, char * line, size_t size);
};

extern NetServer netserver;
extern AsyncWebSocket websocket;

#endif
