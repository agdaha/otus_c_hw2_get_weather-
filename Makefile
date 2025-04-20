
all: getWeather

getWeather: main.c
	$(CC) $^ cJSON.c -o $@ -Wall -Wextra -Wpedantic -std=c11 -lcurl

clean:
	rm -f getWeather core

.PHONY: all clean
