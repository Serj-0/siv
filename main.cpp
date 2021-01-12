#include <iostream>
#include <fstream>
#include <cmath>
#include <ctime>
#include <vector>
#include <dirent.h>
#include <algorithm>
#include <pthread.h>
#include <csignal>
#include <chrono>
#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"
#include "SDL_gifwrap/SDL_gifwrap.h"
#include "boost/filesystem.hpp"
using namespace std;
using namespace boost;
using namespace boost::filesystem;

#define SCALE_DEGREE 1.1
#define SHIFT_DEGREE 0.5
#define sivlog if(verbose) cout

filesystem::ifstream ist;
string str;

int SCR_W, SCR_H;
//int IMG_W, IMG_H;
SDL_Rect rectbfr, recttmp, rectscl;

bool fscr = false, border = true;
bool btn, altdn, ctrdn;
bool albmmode = false;
bool tilemode = true;

//TODO replace with point
//int ms.x, ms.y;

struct f_pair{
    float x, y;
};

struct coord{
    int x, y;
}mpos, ms;

struct gifimg{
    int count;
    SDL_Texture** frames;
    long* timetable;
    f_pair* offset;
    int current_frame;
};

struct image{
    string path;
    SDL_Texture* img;
    float scalex, scaley;
    float xoff, yoff;
    int w, h;
    float theta;
    gifimg gif;
};

pthread_t gif_thread;
bool gif_active;
bool gif_rendering;
unsigned int gif_delta = 0;
unsigned long gif_ptime = 0;

SDL_Window* win;
SDL_Renderer* g;

image* curimg;
int diri = 0;
int loaded = 0;

bool rndr;

vector<image> albm;

//config and flags
bool loaddir = false;
bool verbose = false;
int buffer = 20;

void render();
//void render_gif();
void fitimgwin();
inline void centerimg();
void setwinsize(int, int);
void fitwinmon();
inline void addimg(const char*);
void loadimg(int);
void unloadimg(int);
void fitwinimg();
void setimg(int);
void togglefscr();
void fixwintoimg();
void scaleimg(float, float, bool);
void next_img();
void right_img();
void left_img();
void* gif_func(void*);

string lowcase(string str){
    transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return tolower(c); });
    return str;
}

inline bool num(const char& c){
    return c >= 0x30 && c <= 0x39;
}

bool comp_img(const image& a, const image& b){
    string anum = "", bnum = "";
    
    char* ac = const_cast<char*>(&a.path[0]), *bc = const_cast<char*>(&b.path[0]);

//    cout << "[!] comparing " << a.path << " , " << b.path << "\n";
    while(*ac != 0 && *bc != 0){
//        cout << "comparing " << *ac << ", " << *bc << "\n";
        
        if(num(*ac) && num(*bc)){
            anum.push_back(*ac);
            bnum.push_back(*bc);
            
            ac++;
            bc++;
            
            while(num(*ac) || num(*bc)){
                if(num(*ac)){
                    anum.push_back(*ac);
                    ac++;
                }
                
                if(num(*bc)){
                    bnum.push_back(*bc);
                    bc++;
                }
            }
            
            if(anum != bnum) return stod(anum) < stod(bnum);
            anum.clear();
            bnum.clear();
        }else if(*ac != *bc){
            return *ac < *bc;
        }else{
            ac++;
            bc++;
        }
    }
    
    return a.path.size() < b.path.size();
}

//TODO finish rotation
//TODO load multiple images from args
int main(int argc, char** args){
    /* * CREATE WINDOW * */
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF | IMG_INIT_WEBP);
    SDL_GetGlobalMouseState(&mpos.x, &mpos.y);
    win = SDL_CreateWindow("Serj Image Viewer", mpos.x, mpos.y, SCR_W, SCR_H, 0);
    SDL_SetWindowResizable(win, SDL_TRUE);
    g = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawColor(g, 255, 255, 255, 255);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    args++;
    argc--;
    
    //TODO add sort method flag
    while(*args[0] == '-'){
        switch((*args)[1]){
        case 'd':
            loaddir = true;
            break;
        case 'v':
            verbose = true;
            break;
        case 'a':
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
            break;
        }
        args++;
        argc--;
    }
    
//    if(argc > 2){
//        for(int i = 1; i < argc - 1; i++){
//            if(args[i][0] == '-'){
//                switch(args[i][1]){
//                case 'd':
//                    loaddir = true;
//                    break;
//                case 'v':
//                    verbose = true;
//                    break;
//                case 'a':
//                    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
//                    break;
//                }
//            }
//        }
//    }
    
//    char* filearg = args[argc - 1];
    
    /* * LOAD IMAGES * */
    if(!loaddir){
        while(argc--){
            sivlog << "Filing: " << *args << "\n";
            addimg(*(args++));
        }
        loadimg(0);
        setimg(0);
    }else{
        int dic = 0;
        path fug = path(*args).parent_path();
        sivlog << "Image path parent: " << fug.string() << "\n";

        if(!exists(fug)){
            fug = current_path();
            sivlog << "Does not exist. current path: " << fug.string() << "\n";
        }
        
        //TODO maybe replace with different image detection method
        vector<string> exts = {
            ".png",
            ".jpg",
            ".jpeg",
            ".jfif",
            ".gif",
            ".tiff",
            ".webp",
            ".bmp",
            ".ppm",
            ".pgm",
            ".pbm",
            ".pnm"
        };
        
        for(path p : directory_iterator(fug)){
            for(string s : exts){
                if(lowcase(p.extension().string()) == s){
                    addimg(p.string().c_str());
                    sivlog << "Filed: " << p.string() << "\n";
                    break;
                }
            }
        }
        
        sort(albm.begin(), albm.end(), comp_img);
        
        sivlog << "Sorted:\n";
        for(int i = 0; i < albm.size(); i++){
            if(canonical(path(albm[i].path)) == canonical(path(*args))){
                dic = i;
                sivlog << "Selected image -> ";
            }
            sivlog << i << ": " << albm[i].path << "\n";
        }
        
        diri = dic;
        loadimg(diri);
        setimg(diri);
        sivlog << *args << " loaded as main image, diri: " << diri << endl;
    }
    
    SDL_WaitEvent(nullptr);
    
    /* * SIZE WINDOW * */
    if(!tilemode){
        SDL_GetDisplayBounds(0, &rectbfr);
        if(curimg->h > rectbfr.h || curimg->w > rectbfr.w){
            fitwinmon();
        }else if(SCR_W == 0 && SCR_H == 0){
            fitwinmon();
            SCR_W = curimg->w;
            SCR_H = curimg->h;
            SDL_SetWindowSize(win, SCR_W, SCR_H);
            SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            curimg->scalex = curimg->scaley = 1;
        }
    }else{
        SDL_GetRendererOutputSize(g, &SCR_W, &SCR_H);
        sivlog << "Renderer Output size: " << SCR_W << ", " << SCR_H << "\n";
        if(!curimg->gif.frames) fitimgwin();
    }
    
    /* * RUN * */
    SDL_Event e;
    bool run = true;
    
    while(run){
        SDL_WaitEvent(&e);
        
        //TODO implement album mode
        if(albmmode){
            switch(e.type){
            case SDL_KEYDOWN:
//                switch(e.key.keysym.sym){
//                case SDLK_a:
//                }
                break;
            }
            int aw = static_cast<int>(sqrt(albm.size())) + 1;
        }else{
            rndr = false;
            
            switch(e.type){
            case SDL_KEYUP:
                switch(e.key.keysym.sym){
                case SDLK_LALT:
                    altdn = false;
                    break;
                case SDLK_LCTRL:
                    ctrdn = false;
                    break;
                case SDLK_ESCAPE:
                    run = false;
                    break;
                }
                break;
            case SDL_KEYDOWN:
                switch(e.key.keysym.sym){
                case SDLK_b:
                    if(fscr) togglefscr();
                    border = !border;
                    SDL_SetWindowBordered(win, (SDL_bool) border);
                    break;
                case SDLK_f:
                    togglefscr();
                    break;
                case SDLK_a:
                case SDLK_LEFT:
                    curimg->xoff = -50 * SCR_W / (curimg->w * curimg->scalex);
                    curimg->yoff = -50;
                    rndr = true;
                    break;
                case SDLK_d:
                case SDLK_RIGHT:
                    curimg->xoff = 50 * SCR_W / (curimg->w * curimg->scalex) - 100;
                    curimg->yoff = -50;
                    rndr = true;
                    break;
                case SDLK_w:
                case SDLK_UP:
                    curimg->yoff = -50 * SCR_H / (curimg->h * curimg->scaley);
                    curimg->xoff = -50;
                    rndr = true;
                    break;
                case SDLK_s:
                case SDLK_DOWN:
                    curimg->yoff = 50 * SCR_H / (curimg->h * curimg->scaley) - 100;
                    curimg->xoff = -50;
                    rndr = true;
                    break;
                case SDLK_BACKSPACE:
                    curimg->scalex = 1;
                    curimg->scaley = 1;
                    rndr = true;
                    break;
                case SDLK_c:
                    centerimg();
                    break;
                case SDLK_x:
                    curimg->xoff = -50;
                    rndr = true;
                    break;
                case SDLK_v:
                    curimg->yoff = -50;
                    rndr = true;
                    break;
                case SDLK_LALT:
                    altdn = true;
                    break;
                case SDLK_LCTRL:
                    ctrdn = true;
                    break;
                case SDLK_t:
                    if(e.key.keysym.mod & KMOD_SHIFT){
                        curimg->scalex = static_cast<float>(SCR_W) / curimg->w;
                        curimg->scaley = static_cast<float>(SCR_H) / curimg->h;
                        rndr = true;
                    }else{
                        fitimgwin();
                    }
                    break;
                case SDLK_g:
                    fitimgwin();
                    centerimg();
                    break;
                case SDLK_r:
                    curimg->scalex = curimg->scaley = static_cast<float>(SCR_W) / curimg->w;
                    rndr = true;
                    break;
                case SDLK_y:
                    curimg->scalex = curimg->scaley = static_cast<float>(SCR_H) / curimg->h;
                    rndr = true;
                    break;
                case SDLK_EQUALS:
//                    SCR_W *= SCALE_DEGREE - 0.08;
//                    SCR_H *= SCALE_DEGREE - 0.08;
//                    SDL_SetWindowSize(win, SCR_W, SCR_H);
                    scaleimg(SCALE_DEGREE, SCALE_DEGREE, true);
//                    rndr = true;
                    break;
                case SDLK_MINUS:
//                    SCR_W /= SCALE_DEGREE - 0.08;
//                    SCR_H /= SCALE_DEGREE - 0.08;
//                    SDL_SetWindowSize(win, SCR_W, SCR_H);
                    scaleimg(SCALE_DEGREE, SCALE_DEGREE, false);
//                    rndr = true;
                    break;
                case SDLK_COMMA:
                    left_img();
                    break;
                case SDLK_PERIOD:
                    right_img();
                    break;
                case SDLK_i:
                    fixwintoimg();
                    break;
                case SDLK_q:
                    curimg->theta -= 10;
                    rndr = true;
                    break;
                case SDLK_e:
                    curimg->theta += 10;
                    rndr = true;
                    break;
                case SDLK_z:
                    curimg->theta = 0;
                    rndr = true;
                    break;
                }
                break;
            case SDL_QUIT:
                run = false;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if(e.button.button == SDL_BUTTON_LEFT){
                    btn = true;
                    SDL_GetGlobalMouseState(&ms.x, &ms.y);
                    int wx, wy;
                    SDL_GetWindowPosition(win, &wx, &wy);
                    ms.x -= wx;
                    ms.y -= wy;
                }
                break;
            //TODO double click to fullscreen maybe
            case SDL_MOUSEBUTTONUP:
                if(e.button.button == SDL_BUTTON_RIGHT){
                    if(e.button.x > SCR_W - 50 && e.button.y < 50){
                        run = false;
                        break;
                    }
                    
                    switch(e.button.x > SCR_W / 2){
                    case true:
                        right_img();
                        break;
                    case false:
                        left_img();
                        break;
                    }
                    break;
                }
                btn = false;
                break;
            case SDL_MOUSEMOTION:
                if(btn){
                    if(altdn){
                        SCR_W += e.motion.xrel;
                        SCR_H += e.motion.yrel;
                        SDL_SetWindowSize(win, SCR_W, SCR_H);
                    }else if(ctrdn){
                        curimg->theta = atan((mpos.y - curimg->yoff * curimg->scaley) /
                                (mpos.x - curimg->xoff * curimg->scalex));
                    }else if(!border){
                        SDL_GetGlobalMouseState(&mpos.x, &mpos.y);
                        SDL_SetWindowPosition(win, mpos.x - ms.x, mpos.y - ms.y);
                    }else{
                        curimg->xoff += static_cast<float>(e.motion.xrel) / (curimg->w * curimg->scalex) * 100;
                        curimg->yoff += static_cast<float>(e.motion.yrel) / (curimg->h * curimg->scaley) * 100;
                    }
                    rndr = true;
                }
                
                break;
            case SDL_WINDOWEVENT:
                if(e.window.event == SDL_WINDOWEVENT_RESIZED){
                    SCR_W = e.window.data1;
                    SCR_H = e.window.data2;
                    rndr = true;
                }else if(e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED){
                    rndr = true;
                }
                break;
            case SDL_MOUSEWHEEL:
                scaleimg(SCALE_DEGREE, SCALE_DEGREE, e.wheel.y > 0);
                break;
            case SDL_DROPFILE:
//                dirimgs.push_back({e.drop.file, static_cast<int>(albm.size())});
                addimg(e.drop.file);
                diri = albm.size() - 1;
                loadimg(diri);
                setimg(diri);
                fitimgwin();
                sivlog << e.drop.file << " loaded from drop\n";
                break;
            }

            if(rndr) render();
        }
    }

    if(gif_active){
        gif_active = false;
        pthread_join(gif_thread, 0);
    }
    
    for(int i = 0; i < albm.size(); i++) unloadimg(i);
    SDL_DestroyRenderer(g);
    SDL_DestroyWindow(win);
    IMG_Quit();
    SDL_Quit();
    return 0;
}

//TODO actually render it properly
//TODO add other post processes
void render_gif(){
    if(gif_rendering) return;
    gif_rendering = true;
    
    SDL_Texture*& fr = curimg->gif.frames[curimg->gif.current_frame];
    
    int w, h;
    SDL_QueryTexture(fr, nullptr, nullptr, &w, &h);
    
    rectscl =  {static_cast<int>(SCR_W / 2 + curimg->w * curimg->scalex * (curimg->xoff + curimg->gif.offset[curimg->gif.current_frame].x) / 100),
                static_cast<int>(SCR_H / 2 + curimg->h * curimg->scaley * (curimg->yoff + curimg->gif.offset[curimg->gif.current_frame].y) / 100),
                static_cast<int>(w * curimg->scalex),
                static_cast<int>(h * curimg->scaley)};
    
    SDL_Point pp = {static_cast<int>(w * curimg->scalex / 2), static_cast<int>(h * curimg->scaley / 2)};
    
//    SDL_RenderClear(g);
    
    if(curimg->theta){
        SDL_RenderCopyEx(g, fr, NULL, &rectscl, curimg->theta, &pp, SDL_RendererFlip::SDL_FLIP_NONE);
    }else{
        SDL_RenderCopy(g, fr, NULL, &rectscl);
    }
    
    SDL_RenderPresent(g);
    gif_rendering = false;
}

void render(){
    if(curimg->gif.frames){
        render_gif();
        return;
    }
    
    rectscl =  {static_cast<int>(SCR_W / 2 + curimg->w * curimg->scalex * curimg->xoff / 100),
                static_cast<int>(SCR_H / 2 + curimg->h * curimg->scaley * curimg->yoff / 100),
                static_cast<int>(curimg->w * curimg->scalex),
                static_cast<int>(curimg->h * curimg->scaley)};
    
    SDL_Point pp = {static_cast<int>(curimg->w * curimg->scalex / 2), static_cast<int>(curimg->h * curimg->scaley / 2)};
    
    SDL_RenderClear(g);
    
    if(curimg->theta){
        SDL_RenderCopyEx(g, curimg->img, NULL, &rectscl, curimg->theta, &pp, SDL_RendererFlip::SDL_FLIP_NONE);
    }else{
        SDL_RenderCopy(g, curimg->img, NULL, &rectscl);
    }
    
    SDL_RenderPresent(g);
}

float fitconstraints(SDL_Rect rect, coord bounds){
    float scl = 1;
    
    if(static_cast<float>(bounds.x) / static_cast<float>(bounds.y) > static_cast<float>(rect.w) / static_cast<float>(rect.h)){
        scl = static_cast<float>(bounds.x) / static_cast<float>(rect.h);
    }else{
        scl = static_cast<float>(bounds.x) / static_cast<float>(rect.w);
    }
    return 0;
}

void fitimgwin(){
    float scl = 1;

    if(static_cast<float>(SCR_W) / static_cast<float>(SCR_H) > static_cast<float>(curimg->w) / static_cast<float>(curimg->h)){
        scl = static_cast<float>(SCR_H) / static_cast<float>(curimg->h);
    }else{
        scl = static_cast<float>(SCR_W) / static_cast<float>(curimg->w);
    }

    curimg->scalex = curimg->scaley = scl;
    rndr = true;
}

inline void centerimg(){
    curimg->xoff = -50;
    curimg->yoff = -50;
    rndr = true;
}

void setwinsize(int w, int h){
    SCR_W = w;
    SCR_H = h;
    SDL_SetWindowSize(win, SCR_W, SCR_H);
}

void fitwinmon(){
    SDL_Rect r;
    SDL_GetDisplayBounds(0, &r);

    int bdrx = 0;//border ? 10 : 0;
    int bdry = 0;//border ? 77 : 40;

    fitwinimg();
    if(static_cast<float>(r.w) / r.h > static_cast<float>(curimg->w) / curimg->h){
        SCR_W = static_cast<float>(r.h - bdry) / SCR_H * SCR_W;
        SCR_H = r.h - bdry;
    }else{
        SCR_W = r.w - bdrx;
        SCR_H = r.h - bdry;
    }
    SDL_SetWindowSize(win, SCR_W, SCR_H);
    fitimgwin();
    fitwinimg();
    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, border ? 30 : 0);
    rndr = true;
}

//TODO do not add images that are already added
inline void addimg(const char* path){
    albm.push_back({string(path), nullptr, 1, 1, -50, -50});
}

void loadimg(int i){
    image& nimg = albm[i];
    if(!nimg.img && !nimg.gif.frames){
        if(lowcase(path(nimg.path).extension().string()) == ".gif"){
            GIF_Image* gif = GIF_LoadImage(nimg.path.c_str());
            nimg.gif.count = gif->num_frames;
            nimg.gif.frames = new SDL_Texture*[nimg.gif.count];
            nimg.gif.timetable = new long[nimg.gif.count];
            nimg.gif.offset = new f_pair[nimg.gif.count];
            
            nimg.w = gif->width;
            nimg.h = gif->height;
            
            for(int i = 0; i < nimg.gif.count; i++){
                GIF_Frame*& fr = gif->frames[i];
                
                nimg.gif.frames[i] = SDL_CreateTextureFromSurface(g, fr->surface);
                nimg.gif.timetable[i] = fr->delay;
                nimg.gif.offset[i] = {(float) fr->left_offset / gif->width * 100,
                                      (float) fr->top_offset / gif->height * 100};
            }
            
            GIF_FreeImage(gif);
            sivlog << i << ": " << albm[i].path << " loaded (" << nimg.gif.count << ")\n";
        }else{
            nimg.img = IMG_LoadTexture(g, nimg.path.c_str());
            SDL_QueryTexture(nimg.img, nullptr, nullptr, &nimg.w, &nimg.h);
            sivlog << i << ": " << albm[i].path << " loaded\n";
        }
        loaded++;
    }
}

void unloadimg(int i){
    image& img = albm[i];
    if(img.img){
        SDL_DestroyTexture(img.img);
        img.img = nullptr;
        loaded--;
        sivlog << i << ": " << albm[i].path << " unloaded\n";
    }else if(img.gif.frames){
        for(int i = 0; i < img.gif.count; i++){
            SDL_DestroyTexture(img.gif.frames[i]);
        }
        delete[] img.gif.frames;
        img.gif.frames = nullptr;
        delete img.gif.timetable;
        delete img.gif.offset;
        loaded--;
        sivlog << i << ": " << albm[i].path << " unloaded (" << img.gif.count << ")\n";
    }
}

void fitwinimg(){
    setwinsize(curimg->w * curimg->scalex, curimg->h * curimg->scaley);
    centerimg();
}

void setimg(int i){
    if(gif_active){
        gif_active = false;
        pthread_join(gif_thread, 0);
    }
    
    curimg = &albm[i];
    //TODO add img info to title
    SDL_SetWindowTitle(win, ("Siv [" + to_string(i + 1) + "/" + to_string(albm.size()) + "] " + path(curimg->path).filename().string() +
            " " + to_string(curimg->w) + "x" + to_string(curimg->h)).c_str());
    rndr = true;
    
    if(curimg->gif.frames){
        pthread_create(&gif_thread, nullptr, gif_func, nullptr);
        gif_active = true;
        SDL_RenderClear(g);
    }
}

void togglefscr(){
    SDL_SetWindowFullscreen(win, (fscr = !fscr) * SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_GetRendererOutputSize(g, &SCR_W, &SCR_H);
    rndr = true;
}

void fixwintoimg(){
    if(fscr) togglefscr();
    int xb = (curimg->w * curimg->scalex - SCR_W) / 2;
    int yb = (curimg->h * curimg->scaley - SCR_H) / 2;
    int tx, ty;
    SDL_GetWindowPosition(win, &tx, &ty);
    SDL_SetWindowPosition(win, tx - xb, ty - yb);
    setwinsize(curimg->w * curimg->scalex, curimg->h * curimg->scaley);
    centerimg();
    rndr = true;
}

void scaleimg(float x, float y, bool larger){
    if(x > 0){
        if(larger){
            curimg->scalex *= x;
        }else{
            curimg->scalex /= x;
        }
    }

    if(y > 0){
        if(larger){
            curimg->scaley *= y;
        }else{
            curimg->scaley /= y;
        }
    }
    rndr = true;
}

void next_img(){
    bool b = false;
    if(!albm[diri].img){
        SDL_SetWindowTitle(win, "Loading. . .");
        loadimg(diri);
        b = true;
    }
    setimg(diri);
    if(!curimg->gif.frames) fitimgwin();
    rndr = true;
}

void right_img(){
    if(++diri >= albm.size()) diri = 0;
    next_img();
    if(loaded > buffer)
        unloadimg(diri - buffer < 0 ? albm.size() - 1 + (diri - buffer) : diri - buffer);
    rndr = true;
}

void left_img(){
    if(--diri < 0) diri = albm.size() - 1;
    next_img();
    if(loaded > buffer)
        unloadimg(diri + buffer > albm.size() - 1 ? (diri + buffer) % (albm.size() - 1) : diri + buffer);
    rndr = true;
}

void* gif_func(void*){
    using namespace chrono;
    while(gif_active){
        long t = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        int d = t - gif_ptime;
        gif_ptime = t;
        gif_delta += d;
//        sivlog << gif_delta << " <- " << t << "\n";
        if(gif_delta > curimg->gif.timetable[curimg->gif.current_frame]){
            int& fr = curimg->gif.current_frame;
            fr = (fr + 1) * (fr < curimg->gif.count - 1);
//            sivlog << "GIF NEXT FRAME " << fr << "\n";
            gif_delta = 0;
            render();
        }
    }
    return nullptr;
}
