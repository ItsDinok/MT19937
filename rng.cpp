#include <iostream>
#include <vector>
#include <cstdint>
#include <windows.h>
#include <bcrypt.h>
#include <chrono>
#include <cstring>
#include <cstdint>

/* 
----------- KNOWN BUGS ---------- 
-> Distinct seeds can produce the same number
*/

class Generator {
private:
	static constexpr int STATEARRAYLENGTH = 624;
	static constexpr int TWISTCONSTANT = 397;
	// Constant used in twist
	static constexpr uint32_t MATRIXA	= 0x9908b0dfUL;
	// Mask to extract the upper and lower bits
	static constexpr uint32_t UPPERMASK = 0x80000000UL;
	static constexpr uint32_t LOWERMASK = 0x7fffffffUL;

	// State array
	uint32_t MT[STATEARRAYLENGTH];
	int index;

	// Tempering transformation
	static uint32_t Temper(uint32_t y) {
		// ^= applies xor to y, the number is a bit shift right by n places
		// & HEX applies a mask to it
		y ^= (y >> 11);
		y ^= (y << 7)  & 0x9d2c5680UL;
		y ^= (y << 15) & 0xefc60000UL;
		y ^= (y >> 18);
		return y;
	}

	// Twisting modifies the state array
	void Twist() {
		for (int i = 0; i < STATEARRAYLENGTH; ++i) {
			// Combine upper and lower bits of mt[i] and mt[i+1]
			uint32_t y = (MT[i] & UPPERMASK) | (MT[(i+1) % STATEARRAYLENGTH] & LOWERMASK);
			// Twisting formula
			MT[i] = MT[(i + TWISTCONSTANT) % STATEARRAYLENGTH] ^ (y >> 1);
			// If leastsignificant bit of i is 1, apply matrix transformation
			if (y % 2 != 0) {
				MT[i] ^= MATRIXA;
			}
		}
		index = 0;
	}

	uint32_t GetHRNGSeed() {
		uint32_t seed = 0;

		NTSTATUS status = BCryptGenRandom(
			NULL,
			reinterpret_cast<UCHAR*>(&seed),
			sizeof(seed),
			BCRYPT_USE_SYSTEM_PREFERRED_RNG
		);

		if (status != CMC_STATUS_SUCCESS) {
			throw std::runtime_error("Failed to generate a random number.");
		}
		return seed;
	}

	uint32_t HashSeed(uint32_t seed) {
		const uint32_t FNVOFFSETBASIS = 2166136261UL;
		const uint32_t FNV32PRIME = 16777619UL;

		uint32_t hash = FNVOFFSETBASIS;
		// xor least significant byte
		hash ^= (seed & 0xFF);
		hash *= FNV32PRIME;
		// xor next byte of seed
		hash ^= ((seed >> 8) & 0xFF);
		hash *= FNV32PRIME;
		hash ^= ((seed >> 16) & 0xFF);
		hash *= FNV32PRIME;
		hash ^= ((seed >> 24) & 0xFF);
		hash *= FNV32PRIME;
		return hash;
	}

	uint32_t FNV1a(uint32_t hash, const uint8_t *data, size_t length) {
		const uint32_t FNVOFFSETBASIS = 2166136261UL;
		const uint32_t FNV32PRIME = 16777619UL;
		for (size_t i = 0; i < length; ++i) {
			hash ^= data[i];
			hash *= FNV32PRIME;
		}
		return hash;
	}

	uint32_t GetCPUTimingEntropy() {
		uint64_t tsc;
		// Inline assembly for TSC
		__asm__ __volatile__ ("rdtsc" : "=A"(tsc));
		std::cout << "CPU Timing: " << static_cast<uint32_t>(tsc ^ (tsc >> 12));
		return static_cast<uint32_t>(tsc ^ (tsc >> 12));
	}

	uint32_t MixEntropy(uint32_t entropyOne, uint32_t entropyTwo) {
		uint8_t data[8];
		std::memcpy(data, &entropyOne, sizeof(entropyOne));
		std::memcpy(data + 4, &entropyTwo, sizeof(entropyTwo));

		uint32_t mixedEntropy = 0;
		mixedEntropy = FNV1a(mixedEntropy, data, sizeof(data));
		mixedEntropy = FNV1a(mixedEntropy, data, sizeof(data));
		return mixedEntropy;
	}

	uint32_t GetPerformanceCounterEntropy() {
		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);
		std::cout << "Performance Counter: " << static_cast<uint32_t>(counter.QuadPart);
		return static_cast<uint32_t>(counter.QuadPart);
	}

public:
	Generator(uint32_t seed = 0) : index(STATEARRAYLENGTH) {
		// First value of state array is seed
		if (seed == 0) {
			uint32_t temp = GetHRNGSeed();
			auto now = std::chrono::system_clock::now();
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
			temp += static_cast<uint32_t>(ms);
			std::cout << "\nTime " << temp << std::endl;
			uint32_t secondEntropy = MixEntropy(temp, GetPerformanceCounterEntropy());
			std::cout << "Entropy mix one: " << secondEntropy << std::endl;
			uint32_t thirdEntropy = MixEntropy(temp, GetCPUTimingEntropy());
			seed = MixEntropy(temp, thirdEntropy);	
			std::cout << "Final seed: " << seed << std::endl;
		}
		
		MT[0] = seed;
		for (int i = 0; i < STATEARRAYLENGTH; ++i) {
			// Fill state array with recurrence relation
			MT[i] = 1812433253UL * (MT[i - 1] ^ (MT[i - 1] >> 30)) + i;
		}
	}

	uint32_t random() {
		// If state array is exhausted, twist
		if (index >= STATEARRAYLENGTH) {
			Twist();
		}
		// Return tempered state array value AND increment index
		return Temper(MT[index++]);
	}
};

int main() {
	Generator mt;
	for (int i = 0; i < 10; ++i) {
		std::cout << mt.random() << std::endl;
	}


	return 0;
}

