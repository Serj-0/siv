#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
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
#define SHIFT_PX 50.0
#define sivlog if(verbose) cout

//filesystem::ifstream ist;
//string str;

int SCR_W = 500, SCR_H = 500;
//int IMG_W, IMG_H;
SDL_Rect rectbfr, recttmp, rectscl;

bool fscr = false, border = true;
bool btn, altdn, ctrdn, shftdn;
bool albmmode = false;
bool tilemode = true;

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
    bool* overlay;
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
int albmi = 0;
int loaded = 0;

bool rndr;

vector<image> albm;

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

//config and flags
bool loaddir = false;
bool verbose = false;
bool alias = false;
int buffer = 100;

void render();
void render_gif();
void fitimgwin();
inline void centerimg();
void setwinsize(int, int);
void fitwinmon();
inline int addimg(const char*);
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

void add_directory_images(path dir){
    for(path p : directory_iterator(dir)){
        for(string s : exts){
            if(lowcase(p.extension().string()) == s){
                addimg(p.string().c_str());
                sivlog << "Filed: " << p.string() << "\n";
                break;
            }
        }
    }
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

inline void stop_gif_thread(){
    if(gif_active){
        gif_active = false;
        pthread_join(gif_thread, 0);
    }
}

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

    int argi = 1;
    //TODO add sort method flag
    while(argi < argc && args[argi][0] == '-'){
        switch(args[argi][1]){
        case 'd':
            loaddir = true;
            break;
        case 'v':
            verbose = true;
            break;
        case 'a':
            alias = true;
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
            break;
        }
        sivlog << "Argument " << args[argi] << "\n";
        argi++;
    }
    
    /* * LOAD IMAGES * */
    if(!loaddir){
        if(argi < argc){
            while(argi < argc){
                sivlog << "Adding: " << args[argi] << "\n";
                addimg(args[argi++]);
            }
        }else{
            sivlog << "No file argument, reading STDIN\n";
            char in[BUFSIZ];
            while(cin.getline(in, BUFSIZ)){
                sivlog << "Adding: " << in << "\n";
                addimg(in);
            }
        }
        
        if(!albm.size()){
            cout << "No files given!\n";
        }
        
        loadimg(0);
        setimg(0);
    }else{
        sivlog << "Loading directory\n";
        
        int dic = 0;
        path fug;
        bool fugged = false;
        if(argi < argc){
            fug = path(args[argi]).parent_path();
            fugged = true;
        }else{
            fug = current_path();
        }
        sivlog << "Directory: " << fug.string() << "\n";

        if(!exists(fug)){
            fug = current_path();
            fugged = true;
            sivlog << "Path does not exist. current path: " << fug.string() << "\n";
        }
        
        //TODO maybe replace with different image detection method
        add_directory_images(fug);
        
        sort(albm.begin(), albm.end(), comp_img);
        
        if(fugged){
            sivlog << "Sorted:\n";
            for(int i = 0; i < albm.size(); i++){
                if(canonical(path(albm[i].path)) == canonical(path(args[argi]))){
                    dic = i;
                    sivlog << "Selected image -> ";
                }
                sivlog << i << ": " << albm[i].path << "\n";
            }
        }
        
        albmi = dic;
        loadimg(albmi);
        setimg(albmi);
        sivlog << curimg->path << " loaded as main image, diri: " << albmi << endl;
    }
    
    SDL_WaitEvent(nullptr);
    
    /* * SIZE WINDOW * */
    if(!tilemode){
        SDL_GetDisplayBounds(0, &rectbfr);
        if(albm[albmi].h > rectbfr.h || albm[albmi].w > rectbfr.w){
            fitwinmon();
        }else if(SCR_W == 0 && SCR_H == 0){
            fitwinmon();
            SCR_W = albm[albmi].w;
            SCR_H = albm[albmi].h;
            SDL_SetWindowSize(win, SCR_W, SCR_H);
            SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            albm[albmi].scalex = albm[albmi].scaley = 1;
        }
    }else{
        SDL_GetRendererOutputSize(g, &SCR_W, &SCR_H);
        sivlog << "Renderer Output size: " << SCR_W << ", " << SCR_H << "\n";
        fitimgwin();
    }
    
    /* * RUN * */
    SDL_Event e;
    bool run = true;
    
    setimg(albmi);
//    SDL_RenderClear(g);
    
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
                case SDLK_LSHIFT:
                    shftdn = false;
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
//                case SDLK_a:
//                    if(e.key.keysym.mod & KMOD_CTRL){
//                        alias = !alias;
//                        bool r = SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, alias ? "0" : "1", SDL_HINT_OVERRIDE);
//                        sivlog << r << ", alias toggled " << alias << "\n";
//                        rndr = true;
//                        break;
//                    }
                case SDLK_LEFT:
                    if(e.key.keysym.mod & KMOD_SHIFT){
//                        curimg->xoff = -50.0 * curimg->scalex;
                        curimg->xoff = -50.0 * SCR_W / (curimg->w * curimg->scalex);
                        curimg->yoff = -50.0;
                    }else{
                        //50px
//                        curimg->xoff -= SHIFT_DEGREE / curimg->scalex;
                        curimg->xoff -= SHIFT_PX / curimg->w * 100 / curimg->scalex;
                    }
                    rndr = true;
                    break;
                case SDLK_d:
                case SDLK_RIGHT:
                    if(e.key.keysym.mod & KMOD_SHIFT){
                        curimg->xoff = (100.0 * SCR_W / (2 * curimg->w * curimg->scalex) - 100.0);
                        curimg->yoff = -50.0;
                    }else{
//                        curimg->xoff += SHIFT_DEGREE / curimg->scalex;
                        curimg->xoff += SHIFT_PX / curimg->w * 100 / curimg->scalex;
                    }
                    rndr = true;
                    break;
                case SDLK_w:
                    if(e.key.keysym.mod & KMOD_CTRL){
                        curimg->theta = 0;
                        rndr = true;
                        break;
                    }
                case SDLK_UP:
                    if(e.key.keysym.mod & KMOD_SHIFT){
//                        curimg->yoff = -50.0 * curimg->scaley;
                        curimg->yoff = -50.0 * SCR_H / (curimg->h * curimg->scaley);
                        curimg->xoff = -50.0;
                    }else{
//                        curimg->yoff -= SHIFT_PX / curimg->scaley;
                        curimg->yoff -= SHIFT_PX / curimg->h * 100 / curimg->scaley;
                    }
                    rndr = true;
                    break;
                case SDLK_s:
                case SDLK_DOWN:
                    if(e.key.keysym.mod & KMOD_SHIFT){
//                        curimg->yoff = 50.0 * curimg->scaley;
                        curimg->yoff = (100.0 * SCR_H / (2 * curimg->h * curimg->scaley) - 100.0);
                        curimg->xoff = -50.0;
                    }else{
//                        curimg->yoff += SHIFT_PX / curimg->scaley;
                        curimg->yoff += SHIFT_PX / curimg->h * 100 / curimg->scaley;
                    }
                    rndr = true;
                    break;
                case SDLK_z:
                    curimg->scalex = curimg->scaley = 1;
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
                case SDLK_LSHIFT:
                    shftdn = true;
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
                    curimg->theta = 0;
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
                case SDLK_2:
                    scaleimg(SCALE_DEGREE, SCALE_DEGREE, true);
                    break;
                case SDLK_MINUS:
                case SDLK_1:
                    scaleimg(SCALE_DEGREE, SCALE_DEGREE, false);
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
                    if(e.key.keysym.mod & KMOD_SHIFT){
                        int thetan = static_cast<int>(curimg->theta);
                        int r = thetan % 90;
                        thetan /= 90;
                        
                        curimg->theta = ((thetan - 1 * (r == 0) + (thetan < 1) * 4) * 90);
                    }else{
                        curimg->theta -= 5 - (curimg->theta < 5) * 360;
                    }
                    rndr = true;
                    break;
                case SDLK_e:
                    if(e.key.keysym.mod & KMOD_SHIFT){
                        curimg->theta = ((static_cast<int>(curimg->theta) / 90 + 1) * 90) % 360;
                    }else{
                        curimg->theta += 5 - (curimg->theta >= 355) * 360;
                    }
                    rndr = true;
                    break;
                case SDLK_DELETE:
                    if(albm.size() > 1){
                        stop_gif_thread();
                        int boof = albmi;
                        gif_active = false;
                        unloadimg(boof);
                        albm.erase(albm.begin() + boof);
                        left_img();
                    }
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
                        curimg->theta = -atan2(SCR_W / 2 - e.motion.x, SCR_H / 2 - e.motion.y) * 180 / 3.14159;
                    }else if(shftdn){
                        //TODO implement region zoom
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
                }else if(e.window.event == SDL_WINDOWEVENT_EXPOSED){
                    rndr = true;
                }
                break;
            case SDL_MOUSEWHEEL:
                scaleimg(SCALE_DEGREE, SCALE_DEGREE, e.wheel.y > 0);
                break;
            case SDL_DROPFILE:
//                dirimgs.push_back({e.drop.file, static_cast<int>(albm.size())});
                if(addimg(e.drop.file)){
                    albmi = albm.size() - 1;
                    loadimg(albmi);
                    setimg(albmi);
                    fitimgwin();
                    sivlog << e.drop.file << " loaded from drop\n";
                }else{
                    for(int i = 0; i < albm.size(); i++){
                        if(strcmp(albm[i].path.c_str(), e.drop.file)){
                            albmi = i;
                            setimg(albmi);
                        }
                    }
                }
                break;
            }

            if(rndr) render();
        }
    }

    stop_gif_thread();
    
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
    
    if(!curimg->gif.overlay[curimg->gif.current_frame]) SDL_RenderClear(g);
    
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
    sivlog << "rendered " << clock() << "\n";
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
    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, border ? 30 : 0);
    rndr = true;
}

/**
 * @return 1 if added, 0 if not
 */
inline int addimg(const char* path){
    for(image i : albm){
        if(!strcmp(i.path.c_str(), path)) return 0;
    }
    albm.push_back({string(path), nullptr, 1, 1, -50, -50});
    return 1;
}

//TODO load gif frames with rastered overlay
void loadimg(int i){
    image& nimg = albm[i];
    if(!nimg.img && !nimg.gif.frames){
        if(lowcase(path(nimg.path).extension().string()) == ".gif"){
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
            GIF_Image* gif = GIF_LoadImage(nimg.path.c_str());
            nimg.gif.count = gif->num_frames;
            
            nimg.gif.frames = new SDL_Texture*[nimg.gif.count];
            nimg.gif.timetable = new long[nimg.gif.count];
            nimg.gif.offset = new f_pair[nimg.gif.count];
            nimg.gif.overlay = new bool[nimg.gif.count];
            
            nimg.w = gif->width;
            nimg.h = gif->height;
            
            for(int i = 0; i < nimg.gif.count; i++){
                GIF_Frame*& fr = gif->frames[i];
  
                nimg.gif.frames[i] = SDL_CreateTextureFromSurface(g, fr->surface);
                nimg.gif.timetable[i] = fr->delay + (!fr->delay * 100);
                nimg.gif.offset[i] = {(float) fr->left_offset / gif->width * 100,
                                      (float) fr->top_offset / gif->height * 100};
                nimg.gif.overlay[i] = gif->frames[i]->overlay_previous;
            }
            
            GIF_FreeImage(gif);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, alias ? "0" : "1");
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
//        img.gif.frames = nullptr;
        delete img.gif.timetable;
        delete img.gif.offset;
        delete img.gif.overlay;
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
    SDL_SetWindowTitle(win, ("Siv [" + to_string(i + 1) + "/" + to_string(albm.size()) + "] " + path(curimg->path).filename().string() +
            " " + to_string(curimg->w) + "x" + to_string(curimg->h)).c_str());
    rndr = true;

    if(curimg->gif.frames){
        curimg->gif.current_frame = 0;
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
    if(!albm[albmi].img && !albm[albmi].gif.frames){
        SDL_SetWindowTitle(win, ("Loading " + albm[albmi].path + ". . .").c_str());
        loadimg(albmi);
        b = true;
    }
    setimg(albmi);
    if(b) fitimgwin();
}

void right_img(){
    if(++albmi >= albm.size()) albmi = 0;
    next_img();
    if(loaded > buffer)
        unloadimg(albmi - buffer < 0 ? albm.size() - 1 + (albmi - buffer) : albmi - buffer);
    rndr = true;
}

void left_img(){
    if(--albmi < 0) albmi = albm.size() - 1;
    next_img();
    if(loaded > buffer)
        unloadimg(albmi + buffer > albm.size() - 1 ? (albmi + buffer) % (albm.size() - 1) : albmi + buffer);
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
