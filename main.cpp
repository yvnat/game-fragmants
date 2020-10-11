//2018.12.09
 #include <iostream>
 #include <string>
 #include <vector>
 #include <cmath>
 #include <unordered_set>
 #include "time.h"
 #include <chrono>
 #include <queue>
 #include <stack>
 #include <fstream>
 #include <list>
 
 using namespace std;
 
 #include "CRI.hpp"
 #include "Entity.hpp"
 #include "Mapgen.hpp"
 #include "random.hpp"
 #include "Faction.hpp"

#define RENDERX 21
#define RENDERY 21
#define RENDERRAD 21

#define RENDER_CIRCLE

#define UI_BACK {21, 25, 26}
#define UI_FRAME {46, 44, 44}	//35 34 34
#define UI_TEXT_DULL {56, 55, 55}
#define UI_TEXT_BRIGHT {177, 167, 152} //{158, 134, 100}

class Log {
	class Message {
	public:
		string message;
		array<uint8_t, 3> colour;
		bool isNew;
		Message(string message, array<uint8_t, 3> colour) {
			this->message = message;
			this->colour = colour;
			isNew = true;
		}
	};
	list<Message> messages;
	CRI * console;
public:
	int x1, x2, y1, y2;
	Log(CRI * c, int x1, int y1, int x2, int y2) {
		console = c;
		//ensures x/y1 <= x/y2
		if (x1 > x2) {
			int temp = x2;
			x2 = x1;
			x1 = temp;
		}
		if (y1 > y2) {
			int temp = y2;
			y2 = y1;
			y1 = temp;
		}
		this->x1 = x1;
		this->y1 = y1;
		this->x2 = x2;
		this->y2 = y2;
	}
	void render() {
		console->drawSquare(x1, y1, x2, y2, "LOG", UI_FRAME, UI_BACK, UI_TEXT_BRIGHT);
		console->putC(x1, y1 + 1, 175, UI_TEXT_BRIGHT, UI_BACK);
		int i = 0;
		for (auto it = messages.begin(); it != messages.end() && i < (y2 - y1) - 2; ++it, ++i) {
			array<uint8_t, 3> fore = it->colour;
			if (not it->isNew) {
				fore = console->approachColour(fore, UI_TEXT_DULL, .7);
			}
			console->putString(x1 + 1, y1 + i + 1, it->message, fore, UI_BACK);
		}
	}
	void clear() {
		messages.clear();
	}
	void write(string message, array<uint8_t, 3> colour = UI_TEXT_BRIGHT) {
		messages.emplace_front(message, colour);
	}
	void tick() {
		for (auto it = messages.begin(); it != messages.end(); ++it) {
			if (it->isNew) {
				it->isNew = false;
			} else {
				return;
			}
		}
	}
};

class Dungeon {
public:
	/* 
		auto begin = chrono::high_resolution_clock::now();

		auto end = chrono::high_resolution_clock::now();    
		auto dur = end - begin;
		auto ms = chrono::duration_cast<std::chrono::milliseconds>(dur).count();
	 */
	bool noClip = false;	//lets you walk through walls
	bool allVision = false;	//lets you see everything
	chrono::high_resolution_clock::time_point gameLoopBegin = chrono::high_resolution_clock::now();
	chrono::high_resolution_clock::time_point gameLoopEnd = chrono::high_resolution_clock::now();
   /*---------------------------------Subclasses----------------------------------------------*/
	class Tile {
	public:
		char glyph;
		array<uint8_t, 3> fore, back;
		string description;	//this is what you get when you examine it

		bool blocksLight, blocksMovement, discovered;
		float movementCost;

		Tile(char glyph, string description, bool blocksLight, bool blocksMovement, float movementCost, array<uint8_t, 3> fore, array<uint8_t, 3> back) {
			this->glyph = glyph;
			this->fore = fore;
			this->back = back;
			this->description = description;
			this->blocksLight = blocksLight;
			this->blocksMovement = blocksMovement;
			this->movementCost = movementCost;
			this->discovered = false;
		}
		array<uint8_t, 3> getHiddenFore() {
			float bright = (0.299*static_cast<float>(fore[0]) + 0.587*static_cast<float>(fore[1]) + 0.114*static_cast<float>(fore[2])) / 170.0;
			return {56 * bright, 55 * bright, 55 * bright};
			// return {56, 55, 55};
		}
		array<uint8_t, 3> getHiddenBack() {
			float bright = (0.299*static_cast<float>(back[0]) + 0.587*static_cast<float>(back[1]) + 0.114*static_cast<float>(back[2])) / 170.0;
			return {56 * bright, 55 * bright, 55 * bright};
		}
	};
	class Map {
	public:
		vector< vector<Tile> > map;
		Map() {
			map = {};
		}
		void debugDisplay(CRI * console) {
			for (int i = 0; i < map.size(); ++i) {
				for (int j = 0; j < map[0].size(); ++j) {
					console->putC(j, i, map[i][j].glyph, map[i][j].fore, map[i][j].back);
				}
			}
		}
	};
	class MenuOption {
	public:
		string name;
		void * data;
		MenuOption(string name, void * data) {
			this->name = name;
			this->data = data;
		}
	};
	enum PLAYER_ACTIONS {
		MOVE_UP,	//0
		MOVE_DOWN,
		MOVE_LEFT,
		MOVE_RIGHT,
		MOVE_UL,
		MOVE_UR,	//5
		MOVE_DL,
		MOVE_DR,
		WAIT,
		EQUIP,
		REMOVE,	//10
		GRAB,
		SCREENSHOT,
		EXAMINE,	//13
		__PA_ERROR	//this is to signifiy that an error has occured and is not an actual control.
	};
   /*---------------------------------keybinds------------------------------------------------*/

	/*******************read keybinds**********************/
	PLAYER_ACTIONS stringToPlayerAction(string s) {
		return static_cast<PLAYER_ACTIONS>(stoi(s));
	}
	SDL_Keycode stringToKeycode(string s) {
		return static_cast<SDL_Keycode>(stoi(s));
	}

	pair<SDL_Keycode, PLAYER_ACTIONS> lineToKeybind(string line) {
		//reads into keycode until is reaches a colon, then switches to action
		string keycode = "", action = "";
		bool reachedColon = false;
		for (int i = 0; i < line.size(); ++i) {
			if (line[i] == ' ' || line[i] == '\n') {
				break;
			}
			if (line[i] == ':') {
				reachedColon = true;
				continue;
			}
			if (reachedColon) {
				action += line[i];
			} else {
				keycode += line[i];
			}
		}
		//then returns them converted into an actual keycode and action
		return {stringToKeycode(keycode), stringToPlayerAction(action)};
	}

	unordered_map<SDL_Keycode, PLAYER_ACTIONS> setKeybindsFromFile(string path) {
		cout << "[___]Loading keybinds...";
		unordered_map<SDL_Keycode, PLAYER_ACTIONS> keybinds;
		string currentLine = "";
		ifstream file(path);
		if (file.is_open()) {
			while (getline (file,currentLine)) {
				keybinds.insert(lineToKeybind(currentLine));
			}
			file.close();
		}
		else {
			cout << "\n\n[!!!]ERROR: CANNOT OPEN KEYBINDS FILE TO READ\n\n";
		}
		cout << " successfully loaded\n";
		return keybinds;
	}

	/*******************write keybinds**********************/

	string playerActionToString(PLAYER_ACTIONS action) {
		return to_string(static_cast<int>(action));
	}

	string keycodeToString(SDL_Keycode keycode) {
		return to_string(static_cast<int>(keycode));
	}

	string keybindToLine(pair<SDL_Keycode, PLAYER_ACTIONS> keybind) {
		return keycodeToString(keybind.first) + ":" + playerActionToString(keybind.second);
	}

	void makeFileFromKeybinds(string path, unordered_map<SDL_Keycode, PLAYER_ACTIONS> keybinds) {
		ofstream file(path, ios::out | ios::trunc);
		if (file.is_open()) {
			for (auto i: keybinds) {
				file << keybindToLine(i) + "\n";
			}
			file.close();
		} else {
			cout << "\n\n[!!!]ERROR: CANNOT OPEN KEYBINDS FILE TO WRITE\n\n";
		}
	}

	string playerActionToReadable(PLAYER_ACTIONS action) {
		unordered_map<PLAYER_ACTIONS, string> actions;
		actions[MOVE_UP] = "move north";
		actions[MOVE_DOWN] = "move south";
		actions[MOVE_RIGHT] = "move east";
		actions[MOVE_LEFT] = "move west";
		actions[MOVE_UR] = "move northeast";
		actions[MOVE_UL] = "move northwest";
		actions[MOVE_DR] = "move southeast";
		actions[MOVE_DL] = "move southwest";
		actions[WAIT] = "wait";
		actions[EQUIP] = "equip";
		actions[REMOVE] = "unequip";
		actions[GRAB] = "grab";
		actions[SCREENSHOT] = "screenshot";
		actions[EXAMINE] = "examine";

		if (actions.count(action) == 1) {
			return actions[action];
		} else {
			return "UNNAMED action";
		}
	}

   /*---------------------------------Members + Constructor-----------------------------------*/
	CRI * console;
	Log log = Log(nullptr, 0, 0, 0, 0);

	vector<Entity *> activeEntities;	//this is a subset of entities that get looped to act
	vector<Entity *> entities;			//this stores ALL the entities
	unordered_map<Pos, Entity *> solidEntities;	//this stores all collidable entities. Used for pathfinding.

	vector<Faction *> factions;		//stores all the factions
	vector<Map> dungeon;

	unordered_map<SDL_Keycode, PLAYER_ACTIONS> keybinds;

	void setDefaultKeybinds() {
		//arrows
		keybinds[SDLK_UP] = MOVE_UP;
		keybinds[SDLK_DOWN] = MOVE_DOWN;
		keybinds[SDLK_LEFT] = MOVE_LEFT;
		keybinds[SDLK_RIGHT] = MOVE_RIGHT;

		//vim keys
		keybinds[SDLK_k] = MOVE_UP;
		keybinds[SDLK_j] = MOVE_DOWN;
		keybinds[SDLK_h] = MOVE_LEFT;
		keybinds[SDLK_l] = MOVE_RIGHT;
		keybinds[SDLK_y] = MOVE_UL;
		keybinds[SDLK_u] = MOVE_UR;
		keybinds[SDLK_b] = MOVE_DL;
		keybinds[SDLK_n] = MOVE_DR;

		keybinds[SDLK_PERIOD] = WAIT;
		keybinds[SDLK_e] = EQUIP;
		keybinds[SDLK_r] = REMOVE;
		keybinds[SDLK_g] = GRAB;
		keybinds[SDLK_F2] = SCREENSHOT;
		keybinds[SDLK_SLASH] = EXAMINE;
	}

	Dungeon(CRI * console) {
		this->console = console;
		this->log = Log(console, 0, RENDERY + RENDERRAD, console->X_SIZE, console->Y_SIZE);
		// setDefaultKeybinds();
		keybinds = setKeybindsFromFile("data/keybinds.txt");
		if (keybinds.size() == 0) {
			setDefaultKeybinds();
			cout << "[!!!]ERROR LOADING KEYBINDS: keybinds failed to load. Using default. Functionality may be limited.\n";
		} else {
			cout << "[___]Keybinding successfully loaded from file.\n";
		}
		cout << "[___]successfuly initialized dungeon instance.\n";
	}
   /*---------------------------------Rendering-----------------------------------------------*/
	//tiles that are within FOV in current rendering run
	unordered_set<Pos> visibleTiles;
	unordered_set<Pos> rememberedTiles;
	unordered_set<Pos> visibleEntities;

	void castRay(int renderAtX, int renderAtY, int depth, int x1, int y1, int x2, int y2, int maxRange, array<uint8_t, 3> back = {0, 255, 255}) {	//bresenham algo
		float distance = 0;
		int originX = x1;
		int originY = y1;
		int deltaX = abs(x2 - x1);
		int deltaY = abs(y2 - y1);
		bool blocked = false;

		//this approximates the distance travelled per step
		#ifndef RENDER_CIRCLE
		float deltaDistance;
		if (deltaX < deltaY) {
			/*
			 change the multiplication in 
			 	"1 + (.41 * (..." 
			 to division: 
			 	"1 + (.41 / (..." 
			 for pretty flowers
			*/
			deltaDistance = 1 + (.41421356237 * (static_cast<float>(deltaX) / static_cast<float>(deltaY)));
		} else {
			deltaDistance = 1 + (.41421356237 * (static_cast<float>(deltaY) / static_cast<float>(deltaX)));
		}
		#endif
		int signX, signY;	//sign as in + or -
		if (x1 < x2) {
			signX = 1;
		} else {
			signX = -1;
		}
		if (y1 < y2) {
			signY = 1;
		} else {
			signY = -1;
		}
		int error;
		if (deltaX > deltaY) {
			error = deltaX / 2;
		} else {
			error = -deltaY / 2;
		}
		while (true) {
			/*----------*/

			#ifndef RENDER_CIRCLE
			distance += deltaDistance;
			#else
			distance = sqrt(pow((originX - x1), 2) + pow((originY - y1), 2));
			#endif

			if ((x1 < 0 || x1 >= dungeon[depth].map[0].size()) || (y1 < 0 || y1 >= dungeon[depth].map.size())) {	//goodbye, memory diving
				break;
			}
			if (distance >= maxRange) {
				break;
			}
			if (dungeon[depth].map[y1][x1].blocksLight) {
				// break;
				blocked = true;
			}

			array<uint8_t, 3> fore, back;
			bool toDraw = true;	//set to false if tile is invisible and undiscovered
			if (not blocked) {
				fore = dungeon[depth].map[y1][x1].fore;
				back = dungeon[depth].map[y1][x1].back;
				dungeon[depth].map[y1][x1].discovered = true;
				visibleTiles.emplace(x1, y1, 0);	//z is treated as always 0 for the sake of tiles and such
			} else {
				if (dungeon[depth].map[y1][x1].discovered) {
					if (visibleTiles.count(Pos(x1, y1, 0)) == 1) {
						toDraw = false;
					} else {
						fore = dungeon[depth].map[y1][x1].getHiddenFore();
						back = dungeon[depth].map[y1][x1].getHiddenBack();
						rememberedTiles.emplace(x1, y1, 0);
					}
				} else {
					toDraw = false;
				}
			}
			if (toDraw) {
				console->putC(renderAtX + (x1 - originX), renderAtY + (y1 - originY), dungeon[depth].map[y1][x1].glyph, fore, back);
			}
			
			/*----------*/
			if (x1 == x2 && y1 == y2) {	//if arrived at destination
				break;
			}
			int errorCopy = error;	//make a copy of error. This is because error is potentially modified.
			if (errorCopy > -deltaX) {
				error -= deltaY;
				x1 += signX;
			}
			if (errorCopy < deltaY) {
				error += deltaX;
				y1 += signY;
			}
		}
	}

	//the idea of using a square and then approximating distance travelled of rays is by kyzarti
	void renderSquare(int renderAtX, int renderAtY, int depth, int x, int y, int radius) {
		int surroundingTiles[8][2] = {{1, 1}, {1, 0}, {1, -1}, {0, 1}, {0, -1}, {-1, 1}, {-1, 0}, {-1, -1}};
		visibleTiles.clear();
		visibleEntities.clear();
		rememberedTiles.clear();
		for (int i = -radius; i <= radius; ++i) {
			castRay(renderAtX, renderAtY, depth, x, y, x + radius, y + i, radius);
			castRay(renderAtX, renderAtY, depth, x, y, x - radius, y + i, radius);
			castRay(renderAtX, renderAtY, depth, x, y, x + i, y + radius, radius);
			castRay(renderAtX, renderAtY, depth, x, y, x + i, y - radius, radius);
		}
		
		//post-proc to add walls
		for (auto i: rememberedTiles) {
			for (int j = 0; j < 8; ++j) {
				if (dungeon[depth].map[i.y + surroundingTiles[j][0]][i.x + surroundingTiles[j][1]].blocksLight) {
					console->putC(renderAtX + (i.x + surroundingTiles[j][1] - x), renderAtY + (i.y + surroundingTiles[j][0] - y), dungeon[depth].map[i.y + surroundingTiles[j][0]][i.x + surroundingTiles[j][1]].glyph, dungeon[depth].map[i.y + surroundingTiles[j][0]][i.x + surroundingTiles[j][1]].getHiddenFore(), dungeon[depth].map[i.y + surroundingTiles[j][0]][i.x + surroundingTiles[j][1]].getHiddenBack());
				}
			}
		}

		unordered_set<Pos> visibleTilesNoWalls = visibleTiles;
		for (auto i: visibleTilesNoWalls) {
			for (int j = 0; j < 8; ++j) {
				if (dungeon[depth].map[i.y + surroundingTiles[j][0]][i.x + surroundingTiles[j][1]].blocksLight) {
					console->putC(renderAtX + (i.x + surroundingTiles[j][1] - x), renderAtY + (i.y + surroundingTiles[j][0] - y), dungeon[depth].map[i.y + surroundingTiles[j][0]][i.x + surroundingTiles[j][1]].glyph, dungeon[depth].map[i.y + surroundingTiles[j][0]][i.x + surroundingTiles[j][1]].fore, dungeon[depth].map[i.y + surroundingTiles[j][0]][i.x + surroundingTiles[j][1]].back);
					visibleTiles.emplace(i.x + surroundingTiles[j][1], i.y + surroundingTiles[j][0], 0);
				}
			}
		}

		//render entities
		for (int i = 0; i < entities.size(); ++i) {
			if (visibleTiles.count(entities[i]->pos) == 1) {	//if the entity is within FOV
				visibleEntities.emplace(entities[i]->pos);
				//set the background to be the current tile's unless the entity has a special background
				array<uint8_t, 3> back;
				if (entities[i]->back == array<uint8_t, 3>{0,0,0}) {
					back = dungeon[depth].map[entities[i]->pos.y][entities[i]->pos.x].back;
				} else {
					back = entities[i]->back;
				}
				//if not a held item, render
				if (entities[i]->entityType != Entity::EntityType::item || static_cast<EntityItem *>(entities[i])->holder == nullptr) {
					console->putC(renderAtX + (entities[i]->pos.x - x), renderAtY + (entities[i]->pos.y - y), entities[i]->glyph, entities[i]->fore, back);
				}
			}
		}
	}
   /*---------------------------------UI------------------------------------------------------*/
	//this draws a visual representation of the entity at the given x, y
	void drawEntity(EntityMob * entity, int atx, int aty, array<uint8_t, 3> back = DEF_BACK) {
		for (int i = 0; i < entity->body.size(); ++i) {
			array<uint8_t, 3> bpColour;
			vector<EntityItemArmour *> bpArmours = entity->getArmoursOfBP(entity->body[i].bpType);	//the armours that cover that bp
			if (!entity->body[i].ok) {	//if bodypart is dead, show red
				bpColour = {39, 23, 17};
			} else if (bpArmours.size() == 0) { //if bp uncovered, show skin colour
				bpColour = entity->skinColour;
			} else {	//if covered, show armour colour
				bpColour = bpArmours[0]->fore;
			}
			switch(entity->body[i].bpType) {
				case Entity::BodyPart_Types::BP_HEAD: {
					console->putC(atx + 1, aty, 222, bpColour, back);
					console->putC(atx + 2, aty, 221, bpColour, back);
					break;
				}
				case Entity::BodyPart_Types::BP_TORSO: {
					console->putC(atx + 1, aty + 1, 203, bpColour, back);
					console->putC(atx + 2, aty + 1, 203, bpColour, back);
					console->putC(atx + 1, aty + 2, 199, bpColour, back);
					console->putC(atx + 2, aty + 2, 182, bpColour, back);
					console->putC(atx + 1, aty + 3, 198, bpColour, back);
					console->putC(atx + 2, aty + 3, 181, bpColour, back);
					break;
				}
				case Entity::BodyPart_Types::BP_ARM_L: {
					console->putC(atx + 3, aty + 1, 191, bpColour, back);
					console->putC(atx + 3, aty + 2, 180, bpColour, back);
					console->putC(atx + 3, aty + 3, 221, bpColour, back);
					break;
				}
				case Entity::BodyPart_Types::BP_ARM_R: {
					console->putC(atx, aty + 1, 218, bpColour, back);
					console->putC(atx, aty + 2, 195, bpColour, back);
					console->putC(atx, aty + 3, 222, bpColour, back);
					break;
				}
				case Entity::BodyPart_Types::BP_LEG_L: {
					console->putC(atx + 2, aty + 4, 186, bpColour, back);
					console->putC(atx + 2, aty + 5, 179, bpColour, back);
					break;
				}
				case Entity::BodyPart_Types::BP_LEG_R: {
					console->putC(atx + 1, aty + 4, 186, bpColour, back);
					console->putC(atx + 1, aty + 5, 179, bpColour, back);
					break;
				}
			}
		}
	}
	
	void PLACEHOLDERdrawInventory(EntityMob * entity, int atx, int aty) {
		for (int i = 0; i < entity->inventory.size(); ++i) {
			console->putString(atx, aty+ i, to_string(i), {255, 255, 255}, DEF_BACK);
			console->putString(atx + 2, aty + i, entity->inventory[i]->name + " " + to_string(entity->inventory[i]->equipped), entity->inventory[i]->fore, DEF_BACK);
		}
	}

	//this function takes a string of option names and a function to call when an option is chosen. Both must return true on quit.
	bool openMenu(SDL_Event * e, vector<MenuOption> options, Pos (Dungeon::*action)(SDL_Event *, MenuOption), int x, int y, string title = "") {
		int selectedOption = 0;
		int longestOption = 0;
		for (int i = 0; i < options.size(); ++i) {
			if (options[i].name.size() > longestOption) {
				longestOption = options[i].name.size();
			}
		}
		while (true) {
			//draw the menu
			console->drawSquare(x, y, x + longestOption + 2, y + options.size() + 2, title, UI_FRAME, UI_BACK);
			for (int i = 0; i < options.size(); ++i) {
				array<uint8_t, 3> fore = UI_TEXT_DULL;
				if (i == selectedOption) {
					fore = UI_TEXT_BRIGHT;
				}
				console->putString(x + 1, y + 1 + i, options[i].name, fore, UI_BACK);
			}
			console->render();
			//handle keypresses
			while ((SDL_PollEvent(e)) != 0) {
				if (e->type == SDL_QUIT) {
					return true;
				}
				if (e->type == SDL_KEYDOWN) {
					//cout << "pressed " << SDL_GetKeyName(e->key.keysym.sym) << "\n";
					if (e->key.keysym.sym == SDLK_ESCAPE) {
						return false;
					}
					if (e->key.keysym.sym == SDLK_RETURN) {
						
						return (this->*action)(e, options[selectedOption]).z == 1;
					}
					if (keybinds.count(e->key.keysym.sym) == 0) {
						continue;
					}
					switch(keybinds[e->key.keysym.sym]) {
						case MOVE_UP: {
							selectedOption = --selectedOption;
							if (selectedOption < 0) {
								selectedOption += options.size();
							}
							break;
						}
						case MOVE_DOWN: {
							selectedOption = (++selectedOption) % options.size();
							break;
						}
					}
				}
			}
		}
	}

	void drawUI(EntityMob * entity) {
		drawEntity(entity, 50, 1);
		PLACEHOLDERdrawInventory(entity, 1, 1);
		log.render();
	}
   /*---------------------------------Map gen-------------------------------------------------*/
	//the basic generation is done in the generator, but this takes care of post-processing and such to turn it into a finished map
	Mapgen generator = Mapgen();

	//takes a char, returns an appropriate tile
	Tile getTileFromChar(char toConvert) {
		if (toConvert == '#') {
			return Tile(176, "a rough stone wall", true, true, 10000, {221, 214, 173}, {94, 102, 98});
		} else if (toConvert == '.') {
			if (random::randomBool(80)) {
				return Tile(random::randomElement<char>({249, '.', ':'}), "rocky floor", false, false, 1, {132, 127, 102}, DEF_BACK);
			}
			return Tile(random::randomElement<char>({247, '~'}), "sandy floor", false, false, 1, {132, 127, 102}, DEF_BACK);
		}
		else {
			return Tile('X', "ERROR!", false, false, 1, {0, 0, 0}, {255, 0, 0});
		}
	}

	void addSimpleCAMap(int dimX, int dimY, int iterations, int initCoverage, int toSurvive, int toBorn) {
		//get a raw CA map from the generator
		vector<vector<char>> rawMap = generator.simpleCA(dimX - 2, dimY - 2, iterations, initCoverage, toSurvive, toBorn);

		//make a buffered map which has a 1 tile border
		vector<vector<char>> bufferedMap;
		for (int i = 0; i < dimY; ++i) {
			bufferedMap.push_back({});
			for (int j = 0; j < dimX; ++j) {
				//if it's on the border
				if (i == 0 || i == dimY - 1 || j == 0 || j == dimX - 1) {
					bufferedMap[i].push_back('#');
				} else {
					bufferedMap[i].push_back(rawMap[i - 1][j - 1]);
				}
			}
		}

		//turn buffered map to usable map format
		Map finishedMap = Map();
		for (int i = 0; i < dimY; ++i) {
			finishedMap.map.push_back({});
			for (int j = 0; j < dimX; ++j) {
				finishedMap.map[i].push_back(getTileFromChar(bufferedMap[i][j]));
			}
		}
		
		//add that map to the dungeon
		dungeon.push_back(finishedMap);
	}

	void PLACEHOLDERspawnRandomItem(int count) {
		for (int i = 0; i < count / 3; ++i) {
			bool correct = false;
			Pos pos;
			while (!correct) {
				pos = Pos(random::randomInt(0, dungeon[0].map[0].size()), random::randomInt(0, dungeon[0].map.size()), 0);
				if (!dungeon[0].map[pos.y][pos.x].blocksMovement) {
					correct = true;
				}
			}
			int theNum = random::randomInt(0, 3);
			if (theNum == 0) {
				vector<pair<Entity::BodyPart_Types, int>> coverage = {make_pair(Entity::BodyPart_Types::BP_TORSO, 50), make_pair(Entity::BodyPart_Types::BP_LEG_L, 100), make_pair(Entity::BodyPart_Types::BP_LEG_R, 100)};
				entities.push_back(new EntityItemArmour('H', "overalls", {77, 77, 120}, 0, 10, coverage));
			} else if (theNum == 1) {
				vector<pair<Entity::BodyPart_Types, int>> coverage = {make_pair(Entity::BodyPart_Types::BP_TORSO, 50), make_pair(Entity::BodyPart_Types::BP_ARM_R, 100), make_pair(Entity::BodyPart_Types::BP_ARM_L, 100)};
				entities.push_back(new EntityItemArmour('T', "sweater", {211, 68, 40}, 0, 10, coverage));
			} else {
				entities.push_back(new EntityMob('g', "entity no. " + to_string(rand() % 500), {20 + random::randomInt(1, 230), 20 + random::randomInt(1, 230), 20 + random::randomInt(1, 230)}, 0, factions[0]));
			}
			entities[i]->pos = pos;
			if (theNum == 2) {
				activeEntities.push_back(entities[i]);
			}
		}
	}
	void addFaction(Faction * faction) {
		factions.push_back(faction);
	}
   /*---------------------------------pathfinding---------------------------------------------*/
	
	float getDistMult(int vector[2]) {
		if (abs(vector[0]) + abs(vector[1]) == 2) {
			return 1.41;
		} else {
			return 1;
		}
	}
	float heuristic(Pos point, Pos destination) {	//manhatten distance. This makes it slightly greedy, which is good
		return abs(point.x - destination.x) + abs(point.y - destination.y);
	}
	stack<Pos> AStar(int x1, int y1, int x2, int y2, int depth) {	
		priority_queue<PosPriority> frontier;
		frontier.push(PosPriority(x1, y1, 0, 0));
		//this holds each pos and the previous pose
		unordered_map<Pos, Pos> came_from;
		//this holds the cost of each pos
		unordered_map<Pos, float> cost_so_far;
		came_from[Pos(x1, y1, 0)] = Pos(x1, y1, 0);
		cost_so_far[Pos(x1, y1, 0)] = 0;
		bool found = false;

		while (!frontier.empty()) {
			//pop the frontier
			Pos current = frontier.top().pos; frontier.pop();
			
			if (current == Pos(x2, y2, 0)) {
				found = true;
				break;
			}

			//loop through adjacent tiles
			int adjacent[8][2] = {{1,0}, {-1,0}, {0,-1}, {0,1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}};
			for (int i = 0; i < 8; ++i) {
				Pos next = Pos(current.x + adjacent[i][0], current.y + adjacent[i][1], 0);
				float new_cost = cost_so_far[current] + dungeon[depth].map[next.y][next.x].movementCost * getDistMult(adjacent[i]);
				//if next not already visited or would have a lower cost from this direction
				if (cost_so_far.count(next) == 0 || new_cost < cost_so_far[next]) {
					if (not dungeon[depth].map[next.y][next.x].blocksMovement && (next == Pos(x2, y2, 0) ||  solidEntities.count(next) == 0)) {
						//add it to the frontier and add to map
						cost_so_far[next] = new_cost;
						came_from[next] = current;
						float priority = new_cost;

						priority += heuristic(next, Pos(x2, y2, 0));
						
						frontier.push(PosPriority(next, priority));
					}
				}
			}
		}
		if (not found) {
			return {};
		}

		stack<Pos> path;
		Pos trace = came_from[Pos(x2, y2, 0)];

		// if (trace != Pos(x1, y1, 0)) {
		// 	path.push(trace);
		// }
		
		while (trace != Pos(x1, y1, 0)) {
			path.push(trace);
			trace = came_from[trace];
			// path.push(trace);
		}

		return path;
	}

	bool canSee(int depth1, int x1, int y1, int depth2, int x2, int y2, int maxRange, bool secondPass = false) {
		//no trans-depth vision
		if (depth1 != depth2) {
			return false;
		}

		/*slightly modified copypaste of ray casting code, which itself is just lightly modified bresenham*/
		bool result = true;

		float distance = 0;
		int originX = x1;
		int originY = y1;
		int deltaX = abs(x2 - x1);
		int deltaY = abs(y2 - y1);
		//this approximates the distance travelled per step
		float deltaDistance;
		if (deltaX < deltaY) {
			deltaDistance = 1 + (.41421356237 * (static_cast<float>(deltaX) / static_cast<float>(deltaY)));
		} else {
			deltaDistance = 1 + (.41421356237 * (static_cast<float>(deltaY) / static_cast<float>(deltaX)));
		}
		int signX, signY;	//sign as in + or -
		if (x1 < x2) {
			signX = 1;
		} else {
			signX = -1;
		}
		if (y1 < y2) {
			signY = 1;
		} else {
			signY = -1;
		}
		int error;
		if (deltaX > deltaY) {
			error = deltaX / 2;
		} else {
			error = -deltaY / 2;
		}
		while (true) {
			distance += deltaDistance;
			//if out of range or wall interferes, can't see
			if (distance >= maxRange) {
				result = false;
				break;
			}
			if (dungeon[depth1].map[y1][x1].blocksLight) {
				result = false;
				break;
			}
			if (x1 == x2 && y1 == y2) {	//if arrived at destination, can see
				result = true;
				break;
			}
			int errorCopy = error;	//make a copy of error. This is because error is potentially modified.
			if (errorCopy > -deltaX) {
				error -= deltaY;
				x1 += signX;
			}
			if (errorCopy < deltaY) {
				error += deltaX;
				y1 += signY;
			}
		}
		//if it's visible, return true. If it's not visible and this is the first check, check from the other direction.
		if (result == true) {
			return true;
		} else if (secondPass == false) {
			return canSee(depth2, x2, y2, depth1, originX, originY, maxRange, true);
		} else {
			//if this is the second check and it's not visible, it's not visible at all.
			return false;
		}
	}

	Pos moveTowards(int x1, int y1, int x2, int y2) {
		Pos vector = Pos(0,0,0);
		if (x1 > x2) {
			vector.x = -1;
		} else if (x2 > x1) {
			vector.x = 1;
		}
		if (y1 > y2) {
			vector.y = -1;
		} else if (y2 > y1) {
			vector.y = 1;
		}
		//this stops it from moving directly into the tile
		if (x1 + vector.x == x2 && y1 + vector.y == y2) {
			vector.y = 0;
			vector.x = 0;
			//z is used as a flag to notify that this is happening
			vector.z = 1;
		}
		return vector;
	}

   /*---------------------------------Entity control------------------------------------------*/
	void addActiveEntity(Entity * entity) {
		activeEntities.push_back(entity);
		entities.push_back(entity);
	}

	void render(EntityMob * entity, int x, int y, int depth, int atx = RENDERX, int aty = RENDERY, int radius = RENDERRAD) {
		console->clear();
		
		renderSquare(atx, aty, depth, x, y, radius);
		drawUI(entity);

		console->render();
	}

	void updateSolidEntities() {
		solidEntities.clear();
		for (int i = 0; i < entities.size(); ++i) {
			if (entities[i]->entityType != Entity::EntityType::item) {
				solidEntities[entities[i]->pos] = entities[i];
			}
		}
	}

	bool doEntityLoop(SDL_Event * e) {
		bool returnValue = false;
		updateSolidEntities();
		for (int i = 0; i < activeEntities.size(); ++i) {
			if (activeEntities[i]->entityType == Entity::EntityType::mob) {
				EntityMob * mob = static_cast<EntityMob *>(activeEntities[i]);
				if (mob->isPlayer) {
					gameLoopEnd = chrono::high_resolution_clock::now();
					auto dur = gameLoopEnd - gameLoopBegin;
					auto ms = chrono::duration_cast<std::chrono::milliseconds>(dur).count();
					log.write("looped in " + to_string(ms));
					// console->putString(50, 50, to_string(ms), DEF_FORE, DEF_BACK);
					returnValue = returnValue || getPlayerInput(mob, e);
					if (returnValue) {
						log.write("QUITTING");
					}
					log.tick();
					gameLoopBegin = chrono::high_resolution_clock::now();
				} else {
					doEntityAI(mob);
				}
			}
		}
		return returnValue;
	}

	//moves the entity by dx dy, or moves entity to x y if those are set
	bool moveEntity(EntityMob * entity, int dx, int dy, int x = 0, int y = 0) {
		if (x != 0 || y != 0) {
			dx = x - entity->pos.x;
			dy = y - entity->pos.y;
		}
		Pos newPos = Pos(entity->pos.x + dx, entity->pos.y + dy, 0);
		if (dungeon[entity->depth].map[newPos.y][newPos.x].blocksMovement) {	//if unwalkable, return false
			return false;
		}
		//if it's a player, check visible entities first. If not, check all entities.
		if (solidEntities.count(newPos) == 1) {
			return false;
		}
		//move the entity and update solidEntities
		solidEntities.erase(entity->pos);
		entity->pos = newPos;
		solidEntities[entity->pos] = entity;
		return true;
	}

	//player input methods
	/* 1 is quit, 2 is escape, 3 is finished with modification */
	Pos changeKeybind(SDL_Event * e, MenuOption option) {
		while(true) {
			while ((SDL_PollEvent(e)) != 0) {
				if (e->type == SDL_QUIT) {
					return Pos(0,0,1);
				}
				if (e->type == SDL_KEYDOWN) {
					if (e->key.keysym.sym == SDLK_ESCAPE) {
						return Pos(0,0,2);
					} else {
						for (auto it = keybinds.begin(); it != keybinds.end(); ++it) {
							if (it->first == static_cast<pair<SDL_Keycode, PLAYER_ACTIONS>*>(option.data)->first && it->second == static_cast<pair<SDL_Keycode, PLAYER_ACTIONS>*>(option.data)->second) {
								keybinds.erase(it);
								break;
							}
						}
						keybinds[e->key.keysym.sym] = static_cast<pair<SDL_Keycode, PLAYER_ACTIONS>*>(option.data)->second;
						return Pos(0,0,3);
					}
				}
			}
		}
	}
	Pos escapeMenu(SDL_Event * e, MenuOption option) {
		if (option.name == "Keybinding") {
			vector<MenuOption> options;
			for (auto& i: keybinds) {
				options.push_back(MenuOption(string(SDL_GetKeyName(i.first)) + ":" +playerActionToReadable(i.second), static_cast<void *>(&i)));
			}
			//open menu, quit if told to
			if (openMenu(e, options, &changeKeybind, 10, 1, "Keybinding") == true) {
				return Pos(0,0,1);
			} else {
				return Pos(0,0,3);
			}
		} else if (option.name == "Quit") {
			return Pos(0,0,1);
		}
	}

	/*z pos flags:
	 * 1: quit game
	 * 2: stop asking for cursor actions*/
	Pos waitForCursor(SDL_Event * e, PLAYER_ACTIONS stopper) {
		while (true) {
			while ((SDL_PollEvent(e)) != 0) {
				if (e->type == SDL_QUIT) {
					return Pos(0,0,1);	//1 is a flag to quit
				}
				if (e->type == SDL_KEYDOWN) {
					//cout << "pressed " << SDL_GetKeyName(e->key.keysym.sym) << "\n";
					if (e->key.keysym.sym == SDLK_ESCAPE) {
						return Pos(0,0,2);	//2 is a flag to stop
					}
					if (keybinds.count(e->key.keysym.sym) == 0) {
						continue;
					}
					if (keybinds[e->key.keysym.sym] == stopper) {
						return Pos(0,0,2);	//2 is a flag to stop
					}
					switch(keybinds[e->key.keysym.sym]) {
						case MOVE_UP: {
							return Pos(0,-1,0);
							break;
						}
						case MOVE_DOWN: {
							return Pos(0,1,0);
							break;
						}
						case MOVE_LEFT: {
							return Pos(-1,0,0);
							break;
						}
						case MOVE_RIGHT: {
							return Pos(1,0,0);
							break;
						}
						case MOVE_UL: {
							return Pos(-1,-1,0);
							break;
						}
						case MOVE_UR: {
							return Pos(1,-1,0);
							break;
						}
						case MOVE_DL: {
							return Pos(-1,1,0);
							break;
						}
						case MOVE_DR: {
							return Pos(1,1,0);
							break;
						}
					}
				}
			}
		}
	}
	bool getPlayerInput(EntityMob * player, SDL_Event * e) {
		bool acted = false;
		render(player, player->pos.x, player->pos.y, player->depth);
		while (not acted) {
			while ((SDL_PollEvent(e)) != 0) {
				if (e->type == SDL_QUIT) {
					return true;
				}
				if (e->type == SDL_KEYDOWN) {
					// cout << "pressed " << SDL_GetKeyName(e->key.keysym.sym) << "\n";
					if (e->key.keysym.sym == SDLK_ESCAPE) {
						console->savescreen();
						vector<MenuOption> options;
						options.push_back(MenuOption("Keybinding", nullptr));
						options.push_back(MenuOption("Quit", nullptr));
						if (openMenu(e, options, &escapeMenu, RENDERX - 5, 1, "Menu")) {
							return true;
						}
						console->loadscreen();
						console->render();
					}
					if (keybinds.count(e->key.keysym.sym) == 0) {
						continue;
					}
					switch(keybinds[e->key.keysym.sym]) {
						case MOVE_UP: {
							acted = moveEntity(player, 0, -1);
							break;
						}
						case MOVE_DOWN: {
							acted = moveEntity(player, 0, 1);
							break;
						}
						case MOVE_LEFT: {
							// if(e->key.keysym.mod & KMOD_SHIFT) {
							// 	player->pos.y--;
							// }
							// if(e->key.keysym.mod & KMOD_CTRL) {
							// 	player->pos.y++;
							// }
							acted = moveEntity(player, -1, 0);
							break;
						}
						case MOVE_RIGHT: {
							acted = moveEntity(player, 1, 0);
							break;
						}
						case MOVE_UL: {
							acted = moveEntity(player, -1, -1);
							break;
						}
						case MOVE_UR: {
							acted = moveEntity(player, 1, -1);
							break;
						}
						case MOVE_DL: {
							acted = moveEntity(player, -1, 1);
							break;
						}
						case MOVE_DR: {
							acted = moveEntity(player, 1, 1);
							break;
						}
						case WAIT: {
							acted = true;
							break;
						}
						case SCREENSHOT: {
							cout << console->getTextScreenshot();
							console->savePictureScreenshot("data/screenshots/");
							break;
						}
						case GRAB: {
							bool pickupSuccessful = false;
							for (int i = 0; i < entities.size(); ++i) {
								if (entities[i]->pos == player->pos) {
									if (entities[i]->entityType == Entity::EntityType::item) {
										if (static_cast<EntityItem *>(entities[i])->holder == nullptr) {
											player->grab(static_cast<EntityItem *>(entities[i]));
											pickupSuccessful = true;
											cout << "picked up item\n";
											break;
										} else {
											cout << "item already held\n";
										}
									} else {
										cout << "not an item\n";
									}
								}
							}
							acted = pickupSuccessful;
							break;
						}
						case EQUIP: {
							cout << "---Enter index to wear: ";
							int input;
							bool success = false;
							cin >> input;
							if (input >= player->inventory.size()) {
								cout << "out of bounds!\n";
								break;
							}
							if (player->inventory[input]->itemType == EntityItem::ItemType::armour) {
								success = player->wear(static_cast<EntityItemArmour *>(player->inventory[input]));
								cout << "was able to wear? " + to_string(success) + "\n";
							} else {
								cout << "that's not wearable\n";
							}
							acted = success;
							break;
						}
						case REMOVE: {
							cout << "---Enter index to remove: ";
							int input;
							bool success = false;
							cin >> input;
							if (input >= player->inventory.size()) {
								cout << "out of bounds!\n";
								break;
							}
							if (player->inventory[input]->itemType == EntityItem::ItemType::armour) {
								success = player->doff(static_cast<EntityItemArmour *>(player->inventory[input]));
								cout << "was able to remove? " + to_string(success) + "\n";
							} else {
								cout << "that's not even wearable, much less removable!\n";
							}
							acted = success;
							break;
						}
						case EXAMINE: {
							console->savescreen();
							int cursorX = player->pos.x, cursorY = player->pos.y;
							bool stop = false;
							while (not stop) {
								string entity = "";
								string tile = "nothing";
								array<uint8_t, 3> backLeft = {0,0,0};
								array<uint8_t, 3> backRight = {0,0,0};
								EntityMob * entityToDraw = nullptr;
								if (visibleEntities.count(Pos(cursorX, cursorY, 0)) == 1) {
									bool isFirst = true;
									entity = "";
									for (int i = 0; i < entities.size(); ++i) {
										if (entities[i]->pos == Pos(cursorX, cursorY, 0)) {
											if (not isFirst) {
												entity += "and ";
											}
											entity += entities[i]->name + " ";
											isFirst = false;
											if (entities[i]->entityType == Entity::EntityType::mob) {
												entity += "(" + static_cast<EntityMob *>(entities[i])->personalName + ") ";
												entityToDraw = static_cast<EntityMob *>(entities[i]);
											}
										}
									}
									entity += "over ";
								}
								if (visibleTiles.count(Pos(cursorX, cursorY, 0)) == 1) {
									tile = dungeon[player->depth].map[cursorY][cursorX].description;
								}
								if (visibleTiles.count(Pos(cursorX + 1, cursorY, 0)) == 1) {
									backRight = dungeon[player->depth].map[cursorY][cursorX + 1].back;
								}
								if (visibleTiles.count(Pos(cursorX - 1, cursorY, 0)) == 1) {
									backLeft = dungeon[player->depth].map[cursorY][cursorX - 1].back;
								}
								console->loadscreen();
								console->putC(RENDERX + (cursorX + 1 - player->pos.x), RENDERY + (cursorY - player->pos.y), '>', {255, 255, 0}, backRight);
								console->putC(RENDERX + (cursorX - 1 - player->pos.x), RENDERY + (cursorY - player->pos.y), '<', {255, 255, 0}, backLeft);
								log.write("You see " + entity + tile, {0, 255, 255});
								if (entityToDraw != nullptr) {
									int offsetX = -1, offsetY = 0;
									console->drawSquare(RENDERX + (cursorX - player->pos.x) + 2 + offsetX, RENDERY + (cursorY - player->pos.y) + offsetY, RENDERX + (cursorX - player->pos.x) + 8 + offsetX, RENDERY + (cursorY - player->pos.y) - 8 + offsetY, "", UI_FRAME, UI_BACK);
									drawEntity(entityToDraw, RENDERX + (cursorX - player->pos.x) + 3 + offsetX, RENDERY + (cursorY - player->pos.y) - 7 + offsetY, UI_BACK);
								}
								log.render();
								console->render();
								
								//handle movement
								Pos vector = waitForCursor(e, EXAMINE);
								if (vector.z == 1) {	//if quit
									return true;
								} else if (vector.z == 2) {	//if stop
									stop = true;
								}
								cursorX+=vector.x; cursorY+=vector.y;
							}
							console->loadscreen();
							drawUI(player);
							console->render();
							break;
						}
					}
				}
			}
		}
		return false;
	}

	void doEntityAI(EntityMob * entity) {
		// cout << "dEAI() 1\n";
		for (int i = 0; i < activeEntities.size(); ++i) {
			// cout << "dEAI() 2\n";
			if (activeEntities[i]->entityType != Entity::EntityType::mob) {
				continue;
			}
			if (entity->faction->enemies.count(static_cast<EntityMob *>(activeEntities[i])->faction) == 0) {
				continue;
			}
			// cout << "dEAI() 3\n";
			//if can see, move directly towards
			if (canSee(entity->depth, entity->pos.x, entity->pos.y, activeEntities[i]->depth, activeEntities[i]->pos.x, activeEntities[i]->pos.y, 20)) {
				entity->back = {255, 0, 0};
			} else {
				entity->back = {0, 255, 255};
			}

			// cout << "dEAI() 4\n";
			if (not entity->isAtDestination()) {
				// cout << "dEAI() 5\n";
				if (moveEntity(entity, 0, 0, entity->path.top().x, entity->path.top().y)) {
					entity->path.pop();
				} else if (random::randomBool(30)) {
					entity->setPath(AStar(entity->pos.x, entity->pos.y, activeEntities[i]->pos.x, activeEntities[i]->pos.y, entity->depth));
					entity->back = {255, 255, 0};
				}
				// cout << "dEAI() 5.5\n";
			} else {
				entity->setPath(AStar(entity->pos.x, entity->pos.y, activeEntities[i]->pos.x, activeEntities[i]->pos.y, entity->depth));
				entity->back = {255, 0, 255};
			}
			// cout << "dEAI() 6\n";
		}
		// cout << "dEAI() done\n";
	}
};

int main(int argc, char* args[]) {
	cout << "[_*_]beginning execution\n";
	srand(time(0));
	CRI c;
	// Dungeon d = Dungeon(&c);
	c.init(80, 50);
	Dungeon d = Dungeon(&c);
	cout << "[_*_]dungeon created; console initialized\n";

	d.addSimpleCAMap(60, 60, 20, 45, 4, 5);
	// d.addSimpleCAMap(1000, 100, 4, 40, 4, 5);
	cout << "[_*_]map created\n";
	d.addFaction(new Faction());
	d.PLACEHOLDERspawnRandomItem(50);
	bool run = true;
	SDL_Event e;
	cout << "[_*_]items, event listener created\n";

	bool correct = false;
	Pos ppos;
	while (!correct) {
		ppos = Pos(random::randomInt(0, d.dungeon[0].map[0].size()), random::randomInt(0, d.dungeon[0].map.size()), 0);
		if (!d.dungeon[0].map[ppos.y][ppos.x].blocksMovement) {
			correct = true;
		}
	}
	d.addFaction(new Faction());	//player is on seperate faction from other entities
	d.factions[0]->makeEnemy(d.factions[1]);
	d.factions[1]->makeEnemy(d.factions[0]);
	d.addActiveEntity(new EntityMob('@', "you", {198, 198, 150}, 0, d.factions[1], EntityMob::EntityMobType::humanoid, true, ppos.x, ppos.y));

	cout << "[_*_]begin game loop\n";
	d.log.write("Press ESC for menu", {255, 255, 0});
	while(run) {
		run = !d.doEntityLoop(&e);
    }
	cout << "[_*_]ENDED game loop\n";
	c.quit();
	cout << "[_*_]quit CRI\n";
	d.makeFileFromKeybinds("data/keybinds.txt", d.keybinds);
	cout << "[_*_]saved keybinds. Finishing program.\n";
	return 0;
}