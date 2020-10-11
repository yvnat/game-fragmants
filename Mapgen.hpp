//2018.12.11
#pragma once

#include <string>
#include <vector>
#include <iostream> 

#include "random.hpp"

using namespace std;

//this class handles map generation. It acts entirely independantly, and returns map portions in the form of 2d string chars.
//Converting from string to Tile is up to Dungeon class
class Mapgen {
private:
	vector<vector<char>> currentMap;

	//this function takes a tile coordinate and CA parameters and determines if it lives or dies next cycle
	bool CA_getSurvive(int x, int y, int numToSurvive, int numToBorn, bool borderAlive = true, char aliveChar = '#') {
		//numToLive is either numToSurvive or numToBorn, depending on whether the tile is alive or not.
		int numToLive;
		// cout << "accessing " << x << "," << y<<  " out of " << currentMap[0].size() << "," << currentMap.size() << "\n";
		if (currentMap[y][x] == '#') {
			numToLive = numToSurvive;
		} else {
			numToLive = numToBorn;
		}
		int numLivingNeighbours = 0;
		for (int i = -1; i <= 1; ++i) {
			for (int j = -1; j <= 1; ++j) {
				//handle out of bounds depending on settings (by default the border is considered always alive)
				if (x+j < 0 || x+j >= currentMap[0].size()) {
					numLivingNeighbours += borderAlive;
					continue;
				}
				if (y+i < 0 || y+i >= currentMap.size()) {
					numLivingNeighbours += borderAlive;
					continue;
				}
				//assuming that tile is within bounds, count its status
				numLivingNeighbours += (currentMap[y+i][x+j] == aliveChar);	//true for alive converts to 1, false for dead converts to 0
			}
		}
		return numLivingNeighbours >= numToLive;
	}

public:
	vector<vector<char>> simpleCA(int dimX, int dimY, int iterations, int initCoverage, int toSurvive, int toBorn) {
		currentMap = {};
		for (int i = 0; i < dimY; ++i) {
			currentMap.push_back({});
			for (int j = 0; j < dimX; ++j) {
				//initCoverage chance of it being wall, otherwise is floor
				if (random::randomBool(initCoverage)) {
					currentMap[i].push_back('#');
				} else {
					currentMap[i].push_back('.');
				}
			}
		}
		for (int it = 0; it < iterations; ++it) {
			//make a temp map
			vector<vector<char>> tempMap = vector<vector<char>>(currentMap.size(), vector<char>(currentMap[0].size(), '.'));
			//set it to be the next iteration
			for (int i = 0; i < currentMap.size(); ++i) {
				for (int j = 0; j < currentMap[0].size(); ++j) {
					// cout << i << "," << j << "\n";
					if (CA_getSurvive(j, i, toSurvive, toBorn)) {
						tempMap[i][j] = '#';
					} else {
						tempMap[i][j] = '.';
					}
				}
			}
			//write to map
			currentMap = tempMap;
		}
		return currentMap;
	}

	Mapgen() {
		currentMap = {};
	}
};