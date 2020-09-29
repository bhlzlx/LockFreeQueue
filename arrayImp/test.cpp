#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#include <thread>
#include <cassert>

#include "LockFreeQueue.h"

constexpr size_t ItemCount = 4096;

ugi::LockFreeQueue<size_t> lfq( ItemCount/16 );
ugi::LockFreeQueue<size_t> lfqv( ItemCount );


void producer1() 
{
	for( size_t i = 0; i<ItemCount/2; ++i) {
		while(true) {
			if(lfq.push(i)) {
				break;
			}
		}
	}
}

void producer2() 
{
	for( size_t i = ItemCount/2; i<ItemCount; ++i) {
		while(true) {
			if(lfq.push(i)) {
				break;
			}
		}
	}
}

void consumer() 
{
	for( size_t i = 0; i<ItemCount/2; ++i) {
		size_t val;
		while(!lfq.pop(val)) {
		}
		lfqv.push(val);
	}
}

int main() 
{
	std::thread p1(producer1);
	std::thread p2(producer2);
	std::thread c1(consumer);
	std::thread c2(consumer);
	//
	p1.join();
	p2.join();
	c1.join();
	c2.join();

	for( size_t i = 0; i<ItemCount; ++i ) {
		size_t val;
		bool rst = lfqv.pop(val);
		assert(rst);
		printf("%zu ", val);
	}

	return 0;
}