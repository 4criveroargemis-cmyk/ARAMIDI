#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <rtmidi/RtMidi.h>
#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <map>

struct PianoKey {
    SDL_FRect rect;
    int midiNote;
    bool isBlack;
    bool pressed = false;
    std::string label;
};

enum Mode { LEARN, FREE };

const std::map<std::string, std::vector<int>> SCALES = {
    {"Major",          {0,2,4,5,7,9,11}},
    {"Minor",          {0,2,3,5,7,8,10}},
    {"Dorian",         {0,2,3,5,7,9,10}},
    {"Phrygian",       {0,1,3,5,7,8,10}},
    {"Lydian",         {0,2,4,6,7,9,11}},
    {"Mixolydian",     {0,2,4,5,7,9,10}},
    {"Harmonic Minor", {0,2,3,5,7,8,11}},
    {"Pentatonic Maj", {0,2,4,7,9}},
    {"Pentatonic Min", {0,3,5,7,10}},
    {"Blues",          {0,3,5,6,7,10}},
    {"Whole Tone",     {0,2,4,6,8,10}},
    {"Diminished",     {0,2,3,5,6,8,9,11}},
};

const std::vector<std::string> SCALE_NAMES = {
    "Major","Minor","Dorian","Phrygian","Lydian",
    "Mixolydian","Harmonic Minor","Pentatonic Maj","Pentatonic Min",
    "Blues","Whole Tone","Diminished"
};

const std::vector<std::string> ROOTS = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};

std::set<int> getScaleNotes(int rootIdx, const std::string& scaleName) {
    std::set<int> notes;
    for (int interval : SCALES.at(scaleName))
        notes.insert((rootIdx + interval) % 12);
    return notes;
}

void drawText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
              float x, float y, SDL_Color color) {
    if (!font) return;
    SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), 0, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FRect rect = { x, y, (float)surf->w, (float)surf->h };
    SDL_RenderTexture(renderer, tex, nullptr, &rect);
    SDL_DestroyTexture(tex);
    SDL_DestroySurface(surf);
}

int hitSelector(float mx, float my, SDL_FRect leftBtn, SDL_FRect rightBtn) {
    if (mx >= leftBtn.x && mx <= leftBtn.x+leftBtn.w &&
        my >= leftBtn.y && my <= leftBtn.y+leftBtn.h) return -1;
    if (mx >= rightBtn.x && mx <= rightBtn.x+rightBtn.w &&
        my >= rightBtn.y && my <= rightBtn.y+rightBtn.h) return 1;
    return 0;
}

void sendNoteOn(RtMidiOut* midi, int note, int velocity = 100) {
    if (!midi) return;
    std::vector<unsigned char> msg = {0x90, (unsigned char)note, (unsigned char)velocity};
    midi->sendMessage(&msg);
}

void sendNoteOff(RtMidiOut* midi, int note) {
    if (!midi) return;
    std::vector<unsigned char> msg = {0x80, (unsigned char)note, 0};
    midi->sendMessage(&msg);
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    // Initialize RtMidi
    RtMidiOut* midiOut = nullptr;
    try {
        midiOut = new RtMidiOut();
        // Find ARAMIDI port
        unsigned int nPorts = midiOut->getPortCount();
        int aramidiPort = -1;
        for (unsigned int i = 0; i < nPorts; i++) {
            std::string portName = midiOut->getPortName(i);
            if (portName.find("ARAMIDI") != std::string::npos) {
                aramidiPort = i;
                break;
            }
        }
        if (aramidiPort >= 0) {
            midiOut->openPort(aramidiPort);
            std::cout << "MIDI conectado: ARAMIDI" << std::endl;
        } else {
            std::cout << "Puerto ARAMIDI no encontrado" << std::endl;
        }
    } catch (...) {
        std::cout << "Error inicializando MIDI" << std::endl;
    }

    const int KEY_W = 48;
    const int KEY_H = 90;
    const int N_WHITE = 9;
    const int MARGIN_X = 20;
    const int MARGIN_BOTTOM = 20;
    const int TOP_SPACE = 230;

    const int PIANO_W = N_WHITE * KEY_W;
    const int WINDOW_W = PIANO_W + MARGIN_X * 2;
    const int WINDOW_H = TOP_SPACE + KEY_H + MARGIN_BOTTOM;
    const int OFFSET_X = (WINDOW_W - PIANO_W) / 2;
    const int PIANO_Y = WINDOW_H - MARGIN_BOTTOM - KEY_H;

    SDL_Window* window = SDL_CreateWindow("ARAMIDI-01", WINDOW_W, WINDOW_H, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);

    TTF_Font* logoFont  = TTF_OpenFont("Fonts/2197 Heavy.ttf", 32);
    TTF_Font* subFont   = TTF_OpenFont("Fonts/NimbusSanL-Bol.otf", 14);
    TTF_Font* labelFont = TTF_OpenFont("Fonts/NimbusSanL-Bol.otf", 10);
    TTF_Font* btnFont   = TTF_OpenFont("Fonts/NimbusSanL-Bol.otf", 10);
    TTF_Font* selFont   = TTF_OpenFont("Fonts/NimbusSanL-Bol.otf", 10);

    std::vector<int> WHITE_MIDI  = {60,62,64,65,67,69,71,72,74};
    std::vector<int> BLACK_MIDI  = {61,63,66,68,70,73};
    std::vector<int> BLACK_SLOTS = {0,1,3,4,5,7};

    std::vector<std::string> WHITE_LABELS = {"A","S","D","F","G","H","J","K","L"};
    std::vector<std::string> BLACK_LABELS = {"W","E","T","Y","U","O"};

    std::vector<SDL_Scancode> WHITE_KEYS_PC = {
        SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
        SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L
    };
    std::vector<SDL_Scancode> BLACK_KEYS_PC = {
        SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_T,
        SDL_SCANCODE_Y, SDL_SCANCODE_U, SDL_SCANCODE_O
    };

    const int BW = (int)(KEY_W * 0.55);
    const int BH = (int)(KEY_H * 0.60);

    std::vector<PianoKey> keys;
    for (int i = 0; i < N_WHITE; i++) {
        PianoKey k;
        k.rect     = { (float)(OFFSET_X + i*KEY_W+1), (float)PIANO_Y, (float)(KEY_W-2), (float)KEY_H };
        k.midiNote = WHITE_MIDI[i];
        k.isBlack  = false;
        k.label    = WHITE_LABELS[i];
        keys.push_back(k);
    }
    for (size_t i = 0; i < BLACK_SLOTS.size(); i++) {
        int slot = BLACK_SLOTS[i];
        float cx = OFFSET_X + (slot+1)*KEY_W - BW/2.0f;
        PianoKey k;
        k.rect     = { cx, (float)PIANO_Y, (float)BW, (float)BH };
        k.midiNote = BLACK_MIDI[i];
        k.isBlack  = true;
        k.label    = BLACK_LABELS[i];
        keys.push_back(k);
    }

    Mode currentMode = FREE;
    int rootIdx  = 0;
    int scaleIdx = 0;
    int octaveShift = 0;

    const float ROW1_Y = 30.0f;
    const float ARROW_W = 20.0f;
    const float ARROW_H = 28.0f;
    const float SEL_W   = 90.0f;
    const float SEL_W2  = 140.0f;

    float rootX = (float)OFFSET_X;
    SDL_FRect rootLeft  = { rootX,                   ROW1_Y, ARROW_W, ARROW_H };
    SDL_FRect rootRight = { rootX+ARROW_W+SEL_W,     ROW1_Y, ARROW_W, ARROW_H };

    float scaleX = rootX + ARROW_W*2 + SEL_W + 16.0f;
    SDL_FRect scaleLeft  = { scaleX,                  ROW1_Y, ARROW_W, ARROW_H };
    SDL_FRect scaleRight = { scaleX+ARROW_W+SEL_W2,   ROW1_Y, ARROW_W, ARROW_H };

    int logoW=0, logoH=0;
    if (logoFont) TTF_GetStringSize(logoFont, "ARAMIDI-01", 10, &logoW, &logoH);
    float logoX = OFFSET_X + PIANO_W - (float)logoW;
    float logoY = (float)(PIANO_Y - 10 - logoH);

    int subW=0, subH=0;
    if (subFont) TTF_GetStringSize(subFont, "Made by Spayro", 14, &subW, &subH);
    float subX = OFFSET_X + PIANO_W - (float)subW;
    float subY = logoY - (float)subH - 4.0f;

    const float BTN_W = 75.0f;
    const float BTN_H = 28.0f;
    SDL_FRect btnLearn = { (float)OFFSET_X,            logoY, BTN_W, BTN_H };
    SDL_FRect btnFree  = { (float)OFFSET_X+BTN_W+8,    logoY, BTN_W, BTN_H };

    bool running = true;
    SDL_Event event;

    SDL_Color colWhite     = {255,255,255,255};
    SDL_Color colDark      = {30, 30, 50, 255};
    SDL_Color colBlueLabel = {170,204,255,255};

    while (running) {
        std::set<int> scaleNotes = getScaleNotes(rootIdx, SCALE_NAMES[scaleIdx]);

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;

            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                float mx = event.button.x;
                float my = event.button.y;

                int dr = hitSelector(mx, my, rootLeft, rootRight);
                if (dr != 0)
                    rootIdx = (rootIdx + dr + (int)ROOTS.size()) % (int)ROOTS.size();

                int ds = hitSelector(mx, my, scaleLeft, scaleRight);
                if (ds != 0)
                    scaleIdx = (scaleIdx + ds + (int)SCALE_NAMES.size()) % (int)SCALE_NAMES.size();

                if (mx >= btnFree.x && mx <= btnFree.x+btnFree.w &&
                    my >= btnFree.y && my <= btnFree.y+btnFree.h)
                    currentMode = FREE;
                if (mx >= btnLearn.x && mx <= btnLearn.x+btnLearn.w &&
                    my >= btnLearn.y && my <= btnLearn.y+btnLearn.h)
                    currentMode = LEARN;

                bool found = false;
                for (auto& k : keys) {
                    if (k.isBlack &&
                        mx >= k.rect.x && mx <= k.rect.x+k.rect.w &&
                        my >= k.rect.y && my <= k.rect.y+k.rect.h) {
                        k.pressed = true;
                        sendNoteOn(midiOut, k.midiNote + octaveShift * 12);
                        found = true; break;
                    }
                }
                if (!found) {
                    for (auto& k : keys) {
                        if (!k.isBlack &&
                            mx >= k.rect.x && mx <= k.rect.x+k.rect.w &&
                            my >= k.rect.y && my <= k.rect.y+k.rect.h) {
                            k.pressed = true;
                            sendNoteOn(midiOut, k.midiNote + octaveShift * 12);
                            break;
                        }
                    }
                }
            }

            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                for (auto& k : keys) {
                    if (k.pressed) {
                        sendNoteOff(midiOut, k.midiNote + octaveShift * 12);
                        k.pressed = false;
                    }
                }
            }

            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                SDL_Scancode sc = event.key.scancode;
                for (size_t i = 0; i < WHITE_KEYS_PC.size(); i++) {
                    if (sc == WHITE_KEYS_PC[i] && !keys[i].pressed) {
                        keys[i].pressed = true;
                        sendNoteOn(midiOut, keys[i].midiNote + octaveShift * 12);
                    }
                }
                for (size_t i = 0; i < BLACK_KEYS_PC.size(); i++) {
                    if (sc == BLACK_KEYS_PC[i] && !keys[N_WHITE+i].pressed) {
                        keys[N_WHITE+i].pressed = true;
                        sendNoteOn(midiOut, keys[N_WHITE+i].midiNote + octaveShift * 12);
                    }
                }
                if (sc == SDL_SCANCODE_Z && octaveShift > -4) octaveShift--;
                if (sc == SDL_SCANCODE_X && octaveShift <  4) octaveShift++;
            }

            if (event.type == SDL_EVENT_KEY_UP) {
                SDL_Scancode sc = event.key.scancode;
                for (size_t i = 0; i < WHITE_KEYS_PC.size(); i++) {
                    if (sc == WHITE_KEYS_PC[i]) {
                        keys[i].pressed = false;
                        sendNoteOff(midiOut, keys[i].midiNote + octaveShift * 12);
                    }
                }
                for (size_t i = 0; i < BLACK_KEYS_PC.size(); i++) {
                    if (sc == BLACK_KEYS_PC[i]) {
                        keys[N_WHITE+i].pressed = false;
                        sendNoteOff(midiOut, keys[N_WHITE+i].midiNote + octaveShift * 12);
                    }
                }
            }
        }

        // Render
        SDL_SetRenderDrawColor(renderer, 26,111,255,255);
        SDL_RenderClear(renderer);

        auto drawBtn = [&](SDL_FRect btn, const std::string& label, bool active) {
            SDL_SetRenderDrawColor(renderer,
                active ? 255 : 10,
                active ? 255 : 50,
                active ? 255 : 140, 255);
            SDL_RenderFillRect(renderer, &btn);
            SDL_Color tc = active ? SDL_Color{26,111,255,255} : SDL_Color{170,204,255,255};
            if (btnFont) {
                int lw=0, lh=0;
                TTF_GetStringSize(btnFont, label.c_str(), label.size(), &lw, &lh);
                drawText(renderer, btnFont, label,
                         btn.x+(btn.w-lw)/2, btn.y+(btn.h-lh)/2, tc);
            }
        };

        auto drawSelector = [&](SDL_FRect leftBtn, SDL_FRect rightBtn,
                                float selW, const std::string& value) {
            SDL_FRect valRect = { leftBtn.x+leftBtn.w, leftBtn.y, selW, leftBtn.h };
            SDL_SetRenderDrawColor(renderer, 10,50,140,255);
            SDL_RenderFillRect(renderer, &valRect);
            SDL_RenderFillRect(renderer, &leftBtn);
            SDL_RenderFillRect(renderer, &rightBtn);
            if (selFont) {
                drawText(renderer, selFont, "<", leftBtn.x+4,  leftBtn.y+4,  colWhite);
                drawText(renderer, selFont, ">", rightBtn.x+4, rightBtn.y+4, colWhite);
                int vw=0, vh=0;
                TTF_GetStringSize(selFont, value.c_str(), value.size(), &vw, &vh);
                drawText(renderer, selFont, value,
                         valRect.x+(selW-vw)/2, valRect.y+(valRect.h-vh)/2, colWhite);
            }
        };

        drawSelector(rootLeft,  rootRight,  SEL_W,  ROOTS[rootIdx]);
        drawSelector(scaleLeft, scaleRight, SEL_W2, SCALE_NAMES[scaleIdx]);
        drawBtn(btnLearn, "LEARN", currentMode == LEARN);
        drawBtn(btnFree,  "FREE",  currentMode == FREE);

        // White keys
        for (auto& k : keys) {
            if (k.isBlack) continue;
            bool inScale = scaleNotes.count(k.midiNote % 12);
            SDL_Color fill;
            if (k.pressed)
                fill = {77,143,255,255};
            else if (currentMode == LEARN && inScale)
                fill = {100,180,255,255};
            else
                fill = {232,232,240,255};
            SDL_SetRenderDrawColor(renderer, fill.r,fill.g,fill.b,fill.a);
            SDL_RenderFillRect(renderer, &k.rect);
            if (labelFont)
                drawText(renderer, labelFont, k.label,
                         k.rect.x+KEY_W/2-6, k.rect.y+KEY_H-20, colDark);
        }

        // Black keys
        for (auto& k : keys) {
            if (!k.isBlack) continue;
            bool inScale = scaleNotes.count(k.midiNote % 12);
            SDL_Color fill;
            if (k.pressed)
                fill = {255,255,255,255};
            else if (currentMode == LEARN && inScale)
                fill = {100,180,255,255};
            else
                fill = {26,111,255,255};
            SDL_SetRenderDrawColor(renderer, fill.r,fill.g,fill.b,fill.a);
            SDL_RenderFillRect(renderer, &k.rect);
            if (labelFont)
                drawText(renderer, labelFont, k.label,
                         k.rect.x+BW/2-5, k.rect.y+BH-18, colBlueLabel);
        }

        // Logo
        if (logoFont)
            drawText(renderer, logoFont, "ARAMIDI-01", logoX, logoY, colWhite);

        // Subtitle
        if (subFont)
            drawText(renderer, subFont, "Made by Spayro", subX, subY, colWhite);

        // Octave display
        if (labelFont) {
            std::string octText = "Octave: " + std::to_string(octaveShift + 4);
            drawText(renderer, labelFont, octText,
                     (float)MARGIN_X, 4.0f, colWhite);
        }

        SDL_RenderPresent(renderer);
    }

    // Turn off all notes before closing
    for (auto& k : keys)
        if (k.pressed) sendNoteOff(midiOut, k.midiNote + octaveShift * 12);

    if (midiOut) {
        midiOut->closePort();
        delete midiOut;
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    if (logoFont)  TTF_CloseFont(logoFont);
    if (subFont)   TTF_CloseFont(subFont);
    if (labelFont) TTF_CloseFont(labelFont);
    if (btnFont)   TTF_CloseFont(btnFont);
    if (selFont)   TTF_CloseFont(selFont);
    TTF_Quit();
    SDL_Quit();
    return 0;
}