#!/bin/sh

valgrind --leak-check=full --track-fds=yes --child-silent-after-fork=yes ./shellma
