#pragma once

#include <unordered_set>
#include "random.hpp"

using namespace std;

class Faction {
	vector<char> consonents, vowels;
public:
	string name;
	unordered_set<Faction *> enemies;
	unordered_set<Faction *> allies;

	void generateLanguage() {
		for (int i = 0; i < 20; ++i) {
			consonents.push_back(random::randomElement<char>({'b','c','d','f','g','h','j','k','l','m','n','p','q','r','s','t','v','w','x','y','z'}));
		}
		for (int i = 0; i < 7; ++i) {
			vowels.push_back(random::randomElement<char>({'a','e','o','u','i','a','e','o','u','i','a','e','o','u','i','a','e','o','u','i','a','e','o','u','i',130,131,132,134,136,137,138,139,140,141,145,147,148,149,150,151}));
		}
	}
	string getWord(int length) {
		string word = "";
		for (int i = 0; i < length; ++i) {
			if (i % 2 == 0) {
				word+=random::randomElement<char>(consonents);
			} else {
				word+=random::randomElement<char>(vowels);
			}
		}
		return word;
	}

	Faction() {
		generateLanguage();
		name = getWord(6); name[0] = toupper(name[0]);
		enemies = {};
		allies = {};
	}

	void makeEnemy(Faction * faction) {
		enemies.insert(faction);
	}
	void makeAlly(Faction * faction) {
		allies.insert(faction);
	}

	bool isEnemy(Faction * faction) {
		return (enemies.count(faction) == 1);
	}
	bool isAlly(Faction * faction) {
		return (allies.count(faction) == 1);
	}
};