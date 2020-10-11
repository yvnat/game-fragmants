//2018.12.09
#pragma once

#include <array>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <stack>

#include "CRI.hpp"
#include "random.hpp"
#include "Faction.hpp"

using namespace std;

//the basic coordinate class
class Pos {
public:
	int x, y, z;
	Pos() {}
	Pos(int x, int y, int z) {
		this->x = x;
		this->y = y;
		this->z = z;
	}
	bool operator< (const Pos& pos) const {	//the exact semantics of "bigger" coord don't matter since this is mostly for containers
		return ((x < pos.x) || (x == pos.x && y < pos.y)) || (x == pos.x && y == pos.y && z < pos.z);
	}
	bool operator== (const Pos& pos) const {
		return (x == pos.x) && (y == pos.y);
	}
	bool operator!= (const Pos& pos) const {
		return (x != pos.x) || (y != pos.y);
	}
};
namespace std {	//this implements hashing of Pos, somehow.
	template <>
	struct hash<Pos> {
		std::size_t operator()(const Pos& p) const {
			using std::size_t;
			using std::hash;
			return ((hash<int>()(p.x) ^ (hash<int>()(p.y) << 1)) >> 1);
		}
	};
}
//this class holds a pos and a priority. Used for pathfinding.
class PosPriority {
public:
	Pos pos; 
	float priority;
	PosPriority(int x, int y, int z, float priority) {
		this->pos.x = x;
		this->pos.y = y;
		this->pos.z = z;
		this->priority = priority;
	}
	PosPriority(Pos pos, float priority) {
		this->pos = pos;
		this->priority = priority;
	}
	bool operator< (const PosPriority& pos) const {
		return (priority > pos.priority);
	}
	bool operator<= (const PosPriority& pos) const {
		return (priority <= pos.priority);
	}
};

class Entity {
public:
	enum BodyPart_Types {
		BP_HEAD,
		BP_TORSO,
		BP_ARM_R,
		BP_ARM_L,
		BP_LEG_R,
		BP_LEG_L
	};
	enum EntityType {
		item,
		mob,
	};
	EntityType entityType;

	Pos pos = Pos();
	int depth;

	char glyph;
	array<uint8_t, 3> fore;
	array<uint8_t, 3> back;
	string name;

	bool lightEmitter;
	array<float, 3> light;

	Entity(char glyph, string name, array<uint8_t, 3> fore,
			int depth, bool lightEmitter, array<float, 3> light) {
		this->glyph = glyph;
		this->name = name;
		this->fore = fore;
		this->back = {0,0,0};
		this->depth = depth;
		this->lightEmitter = lightEmitter;
		this->light = light;
	}
};

///////////////////////////////////////////////////////////////////////////items

class EntityItem : public Entity {
public:
   /*--------------------------------members---------------------------*/
	Entity * holder;	//if null, is on floor
	bool equipped;

	enum ItemType {
		item,
		armour,
	};
	ItemType itemType;

   /*----------------------------intializers-----------------------------------*/
	EntityItem(char glyph, string name, array<uint8_t, 3> fore, int depth) 
	: Entity(glyph, name, fore, depth, false, {1, 1, 1}) {
		this->entityType = Entity::EntityType::item;
		this->holder = nullptr;
		this->equipped = false;
		this->itemType = EntityItem::ItemType::item;
	}
   /*-------------------------------other-----------------------------------*/

	void grab(Entity * who) {
		this->pos = Pos(-1, -1, -1);	//kinda disappear
		holder = who;
		cout << "item " + name + ": picked up by entity " + who->name + "\n";
	}
	void drop() {
		this->pos = holder->pos;	//go to the holder's pos
		holder = nullptr;
	}
	void equip() {
		if (holder == nullptr) {
			cout << "ERROR! TRYING TO EQUIP UNHELD ITEM!\n";
			return;
		}
		equipped = true;
	}
	void unequip() {
		equipped = false;
	}
};

class EntityItemArmour : public EntityItem {
public:
   /*--------------------------------members---------------------------*/
	int protectiveness;
	vector<pair<BodyPart_Types, int>> coverage;	//out of 100
   /*----------------------------intializers-----------------------------------*/
	EntityItemArmour(char glyph, string name, array<uint8_t, 3> fore, int depth, 
			int protectiveness, vector<pair<BodyPart_Types, int>> coverage) :
			EntityItem(glyph, name, fore, depth) {
		this->itemType = EntityItem::ItemType::armour;
		this->protectiveness = protectiveness;
		this->coverage = coverage;
	}
};

///////////////////////////////////////////////////////////////////////////mobs
//this is body part used by EntityMob
class BodyPart {
public:
	string name;
	bool ok;
	bool critical, grab, walk;
	Entity::BodyPart_Types bpType;
	BodyPart(Entity::BodyPart_Types bpType, string name, bool critical = false, bool grab = false, bool walk = false) {
		this->bpType = bpType;
		this->name = name;
		this->ok = true;
		this->critical = critical;
		this->grab = grab;
		this->walk = walk;
	}
	bool operator== (const BodyPart& bp) const {
		return (name == bp.name);
	}
};
namespace std {	//hashing of bodyPart
	template <>
	struct hash<BodyPart> {
		std::size_t operator()(const BodyPart& p) const {
			using std::size_t;
			using std::hash;
			return hash<string>()(p.name);
		}
	};
}

class EntityMob : public Entity {
public:
   /*-----------------------------------Members and Initializer----------------------------------------*/
	enum EntityMobType {
		humanoid,
	};
	bool isPlayer;
	EntityMobType type;

	Faction * faction;
	string personalName;
	array<uint8_t, 3> skinColour = {234, 189, 157};	//to be possibly implemented

	string stance;

	vector<BodyPart> body;
	vector<EntityItemArmour *> wornArmour;
	vector<EntityItem *> inventory;
	stack<Pos> path;

	initBody(EntityMobType type) {
		switch(type) {
			case humanoid: {
				body.emplace_back(Entity::BodyPart_Types::BP_HEAD, "head", true);
				body.emplace_back(Entity::BodyPart_Types::BP_TORSO, "torso", true);
				body.emplace_back(Entity::BodyPart_Types::BP_ARM_L, "l. arm", false, true);
				body.emplace_back(Entity::BodyPart_Types::BP_ARM_R, "r. arm", false, true);
				body.emplace_back(Entity::BodyPart_Types::BP_LEG_L, "l. leg", false, false, true);
				body.emplace_back(Entity::BodyPart_Types::BP_LEG_R, "r. leg", false, false, true);
				break;
			}
		}
	}
	EntityMob(char glyph, string name, array<uint8_t, 3> fore, int depth, Faction * faction, EntityMobType type = humanoid, bool isPlayer = false, int x = 0, int y = 0) 
	: Entity(glyph, name, fore, depth, false, {1, 1, 1}) {
		this->entityType = Entity::EntityType::mob;
		this->isPlayer = isPlayer;
		this->type = type;
		this->faction = faction;
		this->personalName = faction->getWord(5);
		initBody(type);
		if (isPlayer) {
			this->lightEmitter = true;
			this->light = {1, .8, 0};
		}
		this->pos.x = x;
		this->pos.y = y;
	}

   /*-----------------------------------Inventory----------------------------------------*/
	void grab(EntityItem * item) {
		cout << "entity " + name + ": picked up item " + item->name + "\n";
		inventory.push_back(item);
		item->grab(this);
	}
	void drop(EntityItem * item) {
		auto it = find(inventory.begin(), inventory.end(), item);
		if (it != inventory.end()) {
			inventory.erase(it);
			item->drop();
		} else {
			cout << "ERROR: Cannot drop item, not found. Error at line " + to_string(__LINE__) + "\n";
		}
	}

	//armour
	int getCoverageForBP(Entity::BodyPart_Types bp) {
		int coverage = 0;
		//loops through all armour
		for (int i = 0; i < wornArmour.size(); ++i) {
			//for that armour, loop through its coverage
			for (int j = 0; j < wornArmour[i]->coverage.size(); ++j) {
				//if it covers the given bodypart,
				if (wornArmour[i]->coverage[j].first == bp) {
					//add that coverage to the total
					coverage += wornArmour[i]->coverage[j].second;
				}
			}
		}
		return coverage;
	}
	vector<EntityItemArmour *> getArmoursOfBP(Entity::BodyPart_Types bp) {
		vector<EntityItemArmour *> armours;
		//loops through all armour
		for (int i = 0; i < wornArmour.size(); ++i) {
			//for that armour, loop through its coverage
			for (int j = 0; j < wornArmour[i]->coverage.size(); ++j) {
				//if it covers the given bodypart,
				if (wornArmour[i]->coverage[j].first == bp) {
					//add it to the list
					armours.push_back(wornArmour[i]);
				}
			}
		}
		return armours;
	}
	bool wear(EntityItemArmour * armour) {
		//can't wear an already-worn armour
		if (armour->equipped) {
			return false;
		}
		//can't wear something you're not holding!
		if (armour->holder != this) {
			return false;
		}
		//for all bodyparts that armour covers
		for (int i = 0; i < armour->coverage.size(); ++i) {
			//if the remaining coverage for that bodypart is less than what is required by the armour
			if ((100 - getCoverageForBP(armour->coverage[i].first)) < armour->coverage[i].second) {
				//can't wear armour
				return false;
			}
		}
		wornArmour.push_back(armour);
		armour->equip();
		return true;
	}
	//originally named "remove" but that's a C++ keyword and I wanted to be on the safe side. removes armour.
	bool doff(EntityItemArmour * armour) {
		//can't remove an unequipped armour
		if (!armour->equipped) {
			return false;
		}
		//can't remove something you're not holding
		if (armour->holder != this) {
			return false;
		}

		//remove from worn armour list
		for(int i = 0; i < wornArmour.size(); ++i) {
			if (wornArmour[i] == armour) {
				wornArmour.erase(wornArmour.begin() + i);
				break;
			}
		}
		armour->unequip();
		return true;
	}

   /*-----------------------------------Pathing----------------------------------------*/

	bool isAtDestination() {
		return path.size() == 0 || path.top() == Pos(-1, -1, -1);
	}
	int distanceFromDestination() {
		if (path.top() == Pos(-1, -1, -1)) {
			return 0;
		}
		return path.size();
	}
	void setPath(stack<Pos> newPath) {
		path = newPath;
	}
	void clearPath() {
		//-1 -1 -1 shows that it is "empty", this is more efficient than actually clearing it
		path.push(Pos(-1, -1, -1));
	}

};