//written 2018.12.08
//CRI stands for something, but I don't quite remember what it is
//this is a vague conglomerate of all the functions related to SDL or rendering.
#pragma once

#include <SDL.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <time.h>
#include <array>

using namespace std;

#define DEF_FORE {192, 192, 192}
#define DEF_BACK {0, 0, 0}

class CRI {
private:
	//how many rows(y)/columns(x)
	const int CHARSET_NUM_COLUMNS = 16, CHARSET_NUM_ROWS = 16;
	//size, in pixels
	int CHARSET_X, CHARSET_Y;

	vector<char> screenState;	//this stores the char that is in each tile of the screen

	//how many rows/columns there are in the screen
	int numOfRows, numOfColumns;
	//charRenderWidth/Height is how large each char should be DRAWN.
	//1 is null
	int charRenderWidth = 1, charRenderHeight = 1;

	SDL_Window * window = nullptr;	//the actual window
	SDL_Surface * surface = nullptr;	//Surface exists within window and is where things will happen before it is committed to window
	SDL_Renderer * renderer = nullptr;	//a renderer

	SDL_Texture * savedscreen = nullptr;	//CRI can store a single saved screenstate. This allows you to avoid recalculating fov etc in menus and ui

	SDL_Texture * charset = nullptr;

	//use this to load an optimized surface from a bitmap
	SDL_Surface * loadSurface(string path) {
		SDL_Surface * unoptimizedSurface = nullptr;
		unoptimizedSurface = SDL_LoadBMP(path.c_str());
		if (unoptimizedSurface == NULL) {
			cout << "loadSurface(): ERROR loading unoptimized surface from " + path + ": " << SDL_GetError() << "\n";
			return nullptr;
		}

		SDL_Surface * optimizedSurface = SDL_ConvertSurface(unoptimizedSurface, surface->format, 0);

		if (optimizedSurface == NULL) {
			cout << "loadSurface(): ERROR optimizing surface from " + path + ": " << SDL_GetError() << "\n";
			return nullptr;
		}
		SDL_FreeSurface(unoptimizedSurface);
		return optimizedSurface;
	}

	/*---------------------------initializers-------------------------------*/
	//this function initializes the SDL window
	bool initWindow(string windowName = "CRI Window") {
		//Initialize SDL
		if(SDL_Init(SDL_INIT_VIDEO) < 0) {
			cout << "init(): ERROR initializing SDL: " << SDL_GetError() << "\n";
			return false;
		} else {
			//Create window
			window = SDL_CreateWindow(windowName.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
			if (window == NULL) {
				cout << "init(): ERROR creating window: " << SDL_GetError() << "\n" << charRenderWidth << "," << charRenderHeight;
				return false;
			} else {
				renderer = SDL_CreateRenderer(window, -1, 0);
				if (renderer == NULL) {
					cout << "init(): ERROR creating renderer: " << SDL_GetError() << "\n";
					return false;
				} else {
					savedscreen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_TARGET, SCREEN_WIDTH, SCREEN_HEIGHT);
					if (savedscreen == NULL) {
						cout << "init(): ERROR creating savedscreen: " << SDL_GetError() << "\n";
						return false;
					} else {
						//Get window surface
						surface = SDL_GetWindowSurface(window);
						return true;
					}
				}
			}
		}
	}

	//this function takes a path to a .bmp tileset and loads it into charset
	bool loadCharset(string path) {
		//load the charset into a surface
		SDL_Surface * image = loadSurface(path);
		if (image == nullptr) {
			cout << "loadCharset(): ERROR loading charset from " + path + ": " << SDL_GetError() << "\n";
			return false;
		}
		//set black to be transparent colour key
		SDL_SetColorKey(image, SDL_TRUE, SDL_MapRGB(image->format, 0, 0, 0));
		//load it into variable as texture
		charset = SDL_CreateTextureFromSurface(renderer, image);
		SDL_FreeSurface(image);
		//update the size of the charset
		SDL_QueryTexture(charset, nullptr, nullptr, &CHARSET_X, &CHARSET_Y);
		return true;
	}

public:
	int SCREEN_WIDTH, SCREEN_HEIGHT, X_SIZE, Y_SIZE;

	init(int xSize, int ySize) {
		// charRenderWidth = (CHARSET_X / CHARSET_NUM_COLUMNS);
		// charRenderHeight = (CHARSET_Y / CHARSET_NUM_ROWS);
		SCREEN_WIDTH = xSize * charRenderWidth;
		SCREEN_HEIGHT = ySize * charRenderHeight;
		numOfRows = ySize;
		numOfColumns = xSize;
		X_SIZE = xSize;
		Y_SIZE = ySize;
		initWindow("test");
		loadCharset("tiles.bmp");
		if (charRenderWidth == 1) {
			resize((CHARSET_X / CHARSET_NUM_COLUMNS), (CHARSET_Y / CHARSET_NUM_ROWS));
		}
		//initialize screenState
		screenState = {};
		for (int i = 0; i < xSize * ySize; ++i) {
			screenState.push_back(' ');
		}
	}

	void setConsoleTitle(string title) {
		SDL_SetWindowTitle(window, title.c_str());
	}

	//resizes the char width and height, along with the window
	void resize(int charWidth, int charHeight) {
		charRenderWidth = charWidth;
		charRenderHeight = charHeight;
		SCREEN_WIDTH = numOfRows * charRenderWidth;
		SCREEN_HEIGHT = numOfColumns * charRenderHeight;
		SDL_SetWindowSize(window, SCREEN_HEIGHT, SCREEN_WIDTH);
	}
	
	//this method puts given character at x, y on terminal, with given foreground and background colours
	void putC(int x, int y, unsigned char glyph, array<uint8_t, 3> fore = DEF_FORE, array<uint8_t, 3> back = DEF_BACK) {
		//this rect determines the drawing area of the shown section
		SDL_Rect drawrect = {x*charRenderWidth, y*charRenderHeight, charRenderWidth, charRenderHeight};

		//this rect determines which portion of the image will actually be shown
		SDL_Rect cliprect = {(glyph % CHARSET_NUM_COLUMNS) * (CHARSET_X / CHARSET_NUM_COLUMNS), 
							(glyph / CHARSET_NUM_COLUMNS) * (CHARSET_Y / CHARSET_NUM_ROWS), 
							(CHARSET_X / CHARSET_NUM_COLUMNS), (CHARSET_Y / CHARSET_NUM_ROWS)};

		SDL_SetTextureColorMod(charset, fore[0], fore[1], fore[2]);	//this sets foreground colour
		SDL_SetRenderDrawColor(renderer, back[0], back[1], back[2], 255);	//this sets background colour

		SDL_RenderFillRect(renderer, &drawrect);	//draw background
		SDL_RenderCopy(renderer, charset, &cliprect, &drawrect);	//draw foreground

		screenState[x + (y*numOfColumns)] = glyph;	//update the screenState
	}

	//this sets a tile to be a full colour. Stripped down version of putC()
	void fill(int x, int y, array<uint8_t, 3> back) {
		//this rect determines the drawing area of the shown section
		SDL_Rect drawrect = {x*charRenderWidth, y*charRenderHeight, charRenderWidth, charRenderHeight};
		SDL_SetRenderDrawColor(renderer, back[0], back[1], back[2], 255);	//this sets background colour
		SDL_RenderFillRect(renderer, &drawrect);	//draw background
	}

	//this prints a string to the screen over multiple tiles. Similar to putC.
	void putString(int x, int y, string input, array<uint8_t, 3> fore = DEF_FORE, array<uint8_t, 3> back = DEF_BACK) {
		for (int i = 0; i < input.length(); ++i) {
			putC(x+i, y, input[i], fore, back);
		}
	}

	//put the renderer on the screen
	void render() {
		SDL_RenderPresent(renderer);	//this actually draws stuff
	}

	//clears the renderer
	//note that a call to render() is still required to commit the blank canvas to the screen
	void clear() {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);	//set colour to black
		SDL_RenderClear(renderer);
		memset(&screenState[0],' ', numOfColumns*numOfRows);
	}

	//quits SDL.
	void quit() {
		SDL_DestroyTexture(charset);
		SDL_DestroyTexture(savedscreen);
		SDL_FreeSurface(surface);
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	//this returns a text "screenshot" of the screen
	string getTextScreenshot() {
		string toReturn = "";
		for (int i = 0; i < screenState.size(); ++i) {
			if (i % (SCREEN_WIDTH / charRenderHeight) == 0) {
				toReturn += "\n";
			}
			toReturn += screenState[i];
		}
		return toReturn;
	}
	void savePictureScreenshot(string path) {
		SDL_Surface * screenshot = SDL_CreateRGBSurface(0, SCREEN_HEIGHT, SCREEN_WIDTH, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
		SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screenshot->pixels, screenshot->pitch);
		SDL_SaveBMP(screenshot, (path + to_string(time(0)) + ".bmp").c_str());
		SDL_FreeSurface(screenshot);
	}

	void savescreen() {
		SDL_Surface * temp = SDL_CreateRGBSurface(0, SCREEN_HEIGHT, SCREEN_WIDTH, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
		SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, temp->pixels, temp->pitch);
		savedscreen = SDL_CreateTextureFromSurface(renderer, temp);
		SDL_FreeSurface(temp);
	}
	void loadscreen() {
		SDL_RenderCopy(renderer, savedscreen, NULL, NULL);
	}

	vector<SDL_Event> get_events() {
		SDL_Event e;
		vector<SDL_Event> events;
		while(SDL_PollEvent(&e) != 0) {
			events.push_back(e);
		}
		return events;
	}

	void delay(int ms) {
		SDL_Delay(ms);
	}

	void drawSquare(int x1, int y1, int x2, int y2, string title = "", array<uint8_t, 3> border = DEF_FORE, array<uint8_t, 3> back = DEF_BACK, array<uint8_t, 3> titleColour = DEF_FORE) {
		if (y1 > y2) {
			int temp = y1;
			y1 = y2;
			y2 = temp;
		}
		if (x1 > x2) {
			int temp = x1;
			x1 = x2;
			x2 = temp;
		}
		for (int i = y1; i < y2; ++i) {
			for (int j = x1; j < x2; ++j) {
				if (i == y1) {	//top
					if (j == x1) {
						putC(j, i, 218, border, back);
					} else if (j == x2 - 1) {
						putC(j, i, 191, border, back);
					} else if (title != "") {
						if (j == x1 + 1) {
							putC(j, i, 180, border, back);
						} else if (j < x1 + 2 + title.size()) {
							putC(j, i, title[j - x1 - 2], titleColour, back);
						} else if (j == x1 + 2 + title.size()) {
							putC(j, i, 195, border, back);
						} else {
							putC(j, i, 196, border, back);
						}
					} else {
						putC(j, i, 196, border, back);
					}
				} else if (i == y2 - 1) {	//bottom
					if (j == x1) {
						putC(j, i, 192, border, back);
					} else if (j == x2 - 1) {
						putC(j, i, 217, border, back);
					} else {
						putC(j, i, 196, border, back);
					}
				} else if (j == x2 - 1 || j == x1) {
					putC(j, i, 179, border, back);
				} else {
					fill(j, i, back);
				}
			}
		}
	}

	//factor = 1 means bring fully to colour, factor = 0 means no change
	array<uint8_t, 3> approachColour(array<uint8_t, 3> approacher, array<uint8_t, 3> toApproach, float factor) {
		int dR = (toApproach[0] - approacher[0]) * factor;
		int dG = (toApproach[1] - approacher[1]) * factor;
		int dB = (toApproach[2] - approacher[2]) * factor;
		return {approacher[0] + dR, approacher[1] + dG, approacher[2] + dB};
	}
};