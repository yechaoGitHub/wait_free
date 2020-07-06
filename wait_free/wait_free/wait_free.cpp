// wait_free.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "wait_free_queue.hpp"

wait_free_queue<int>	q(5);
std::atomic<int>		count;

void func()
{
	int i(600);

	int arr[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

	while (i--)
	{
		int j = rand() % 100;

		if (j <= 30)
		{
			q.enqueue_range(arr, sizeof(arr) / sizeof(int));
			count += 10;
		}
		else if (j > 30 && j <= 60)
		{
			q.enqueue(1);
			count++;
		}
		else if (j > 60 && j <= 90)
		{
			int j(0);
			q.dequeue(j);
			count--;
		}
		else
		{
			int r = q.dequeue_range(arr, sizeof(arr));
			count -= r;
		}
	}
}

void write_test()
{
	int i(0);

	uint32_t count(10000);

	while (count--)
	{
		int d = rand() % 100;

		if (i < 70)
		{
			assert(q.enqueue(i) == i);
			i++;
		}
		else
		{

			int arr[10] = {};
			for (int& j : arr)
			{
				j = i++;
			}

			auto c = q.enqueue_range(arr, 10);

			assert(c == i - 10);
		}
	}
}

void read_test()
{
	int pre_v(-1);
	int i(0);
	uint64_t index(0);

	while (true)
	{
		int d = rand() % 100;

		if (d < 70)
		{
			if (q.dequeue(i, &index))
			{
				assert(i == index);
				assert(i > pre_v);
				pre_v = i;
				std::cout << i << " ";
			}
		}
		else
		{
			int arr[10] = {};
			uint32_t count = q.dequeue_range(arr, sizeof(arr), &index);
			if (count)
			{
				assert(arr[0] == index);
			}
			for (uint32_t j = 0; j < count; j++)
			{
				std::cout << arr[j] << " ";
				assert(arr[j] > pre_v);
				pre_v = arr[j];
			}
		}
	}
}

int main()
{
	std::thread th1(func);
	std::thread th2(func);
	std::thread th3(func);
	std::thread th4(func);
	std::thread th5(func);
	std::thread th6(func);

	th1.join();
	th2.join();
	th3.join();
	th4.join();
	th5.join();
	th6.join();

	/*std::thread th1(write_test);
	std::thread th2(read_test);

	th1.join();
	th2.join();*/

	return 0;
}

