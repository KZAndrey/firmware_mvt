"""Regression: compile the actual banner functions with a tiny display adapter."""
import os,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
os.chdir(root)
build=root/'.tools/wss-tests'
source=(root/'src/graphics/draw/NotificationRenderer.cpp').read_text(encoding='utf-8-sig')
def function(name):
    start=source.index('void NotificationRenderer::'+name+'(')
    opening=source.index('{',start); depth=1; i=opening+1
    while depth:
        depth += (source[i]=='{')-(source[i]=='}'); i+=1
    return source[start:i]
preamble=r'''
#include <algorithm>
#include <vector>
#include <cstring>
#include <string>
#include <functional>
#include <cstdio>
#include <cstdint>
#define MAX_LINES 5
#define FONT_HEIGHT_SMALL 13
#define FONT_SMALL nullptr
#define TEXT_ALIGN_LEFT 0
#define BLACK 0
#define WHITE 1
#define INPUT_BROKER_NONE 0
#define INPUT_BROKER_UP 1
#define INPUT_BROKER_LEFT 2
#define INPUT_BROKER_ALT_PRESS 3
#define INPUT_BROKER_DOWN 4
#define INPUT_BROKER_RIGHT 5
#define INPUT_BROKER_USER_PRESS 6
#define INPUT_BROKER_SELECT 7
#define INPUT_BROKER_CANCEL 8
#define INPUT_BROKER_ALT_LONG 9
bool isHighResolution=false;
uint32_t millis(){return 1000;}
uint8_t bell_alert[8]={};
struct OLEDDisplayUiState{};
struct OLEDDisplay{
 int h=32; unsigned drawn=0;
 int height(){return h;} int width(){return 64;}
 uint16_t getStringWidth(const char *s,size_t n,bool){for(size_t i=0;i<n;++i){volatile char c=s[i];(void)c;}return n*6;}
 void setFont(const uint8_t*){} void setTextAlignment(int){} void setColor(int){}
 void fillRect(int,int,int,int){} void drawRect(int,int,int,int){}
 void drawXbm(int,int,int,int,const uint8_t*){}
 void drawString(int,int,const char *s){(void)strlen(s);++drawn;}
};
struct NotificationRenderer{
 static inline char alertBannerMessage[256]={};
 static inline unsigned alertBannerOptions=0,alertBannerUntil=0;
 static inline const char **optionsArrayPtr=nullptr;
 static inline const int *optionsEnumPtr=nullptr;
 static inline int curSelected=0;
 struct Event{int inputEvent;}; static inline Event inEvent={0};
 static inline std::function<void(int)> alertBannerCallback=[](int){};
 static void resetBanner(){alertBannerMessage[0]=0;}
 static void drawAlertBannerOverlay(OLEDDisplay*,OLEDDisplayUiState*);
 static void drawNotificationBox(OLEDDisplay*,OLEDDisplayUiState*,const char*[],uint16_t,uint8_t,uint16_t=0);
};
'''
main=r'''
int main(){
 OLEDDisplay d;OLEDDisplayUiState state;
 const char *options[]={"OK","Got it!"};
 for(int h:{32,64}) for(unsigned opts:{0U,2U}){
  d.h=h;d.drawn=0;NotificationRenderer::alertBannerOptions=opts;NotificationRenderer::optionsArrayPtr=options;
  strcpy(NotificationRenderer::alertBannerMessage,"Welcome to Meshtastic!\nClick to navigate and\nlong press to select\nor open a menu.");
  NotificationRenderer::drawAlertBannerOverlay(&d,&state);
  if(!d.drawn)return 2;
 }
 std::string longOption(200,'X');const char *longOptions[]={longOption.c_str()};
 NotificationRenderer::alertBannerOptions=1;NotificationRenderer::optionsArrayPtr=longOptions;
 strcpy(NotificationRenderer::alertBannerMessage,"Title");NotificationRenderer::drawAlertBannerOverlay(&d,&state);
 const char *unterminated[]={"one","two"};
 NotificationRenderer::drawNotificationBox(&d,&state,unterminated,2,0);
 printf("Actual banner regression: 64x32 onboarding, options, long labels and bounded line iteration passed\n");
}
'''
cpp=build/'banner_test.cpp'; cpp.write_text(preamble+function('drawAlertBannerOverlay')+'\n'+function('drawNotificationBox')+main)
env=dict(os.environ,ZIG_LOCAL_CACHE_DIR=str(build/'cache'),ZIG_GLOBAL_CACHE_DIR=str(build/'global-cache'))
zig=str(Path(sys.argv[1]).resolve());exe=build/'banner_test.exe'
with (build/'banner-compile.log').open('w') as log:
 subprocess.run([zig,'c++','-target','x86_64-windows-gnu','-std=c++17','-O0',str(cpp),'-o',str(exe)],env=env,stdout=log,stderr=log,check=True)
subprocess.run([str(exe)],check=True)
