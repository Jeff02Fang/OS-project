all:
	g++ main.cpp Task.cpp Scheduler_On.cpp Scheduler_O1.cpp Logger.cpp ThreadUtils.cpp -o main -pthread -g -fsanitize=address -O0 -Wall -Wextra -std=c++17

short:
	g++ main.cpp Task.cpp Scheduler_On.cpp Scheduler_O1.cpp Logger.cpp ThreadUtils.cpp -o main -pthread -O0 -Wall -Wextra -std=c++17

merge:
	sort -n -k1 cpu*.log io*.log > merged.log
	python3 metrics.py

clean:
	rm -f *.log main analyzer *.csv

log:
	rm -f *.log
	clear