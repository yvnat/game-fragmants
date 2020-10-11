#pragma once;

#include <cstdlib>
#include <vector>

namespace random {
	//[min, max)
	int randomInt(int min, int max) {
		return (std::rand() % (max - min)) + min;
	}

	bool randomBool(int chance) {
		return randomInt(0, 100) + 1 < chance;
	}

	//this returns a random element of a given array
	template<typename T> T randomElement(std::vector<T> array) {
		return array[randomInt(0, array.size())];
	}
}