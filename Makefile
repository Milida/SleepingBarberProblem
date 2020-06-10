compile:
    gcc main.c -pthread -o sem -std=gnu99

.PHONY: clean

clean:
    rm main.c sem