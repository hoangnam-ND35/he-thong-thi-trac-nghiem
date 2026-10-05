FROM debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends gcc libc6-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY c c
COPY sql sql
COPY web web
COPY third_party/sqlite-amalgamation-3460100 third_party/sqlite-amalgamation-3460100
RUN gcc -c -O2 -w -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION -D_GNU_SOURCE \
      -Ithird_party/sqlite-amalgamation-3460100 \
      third_party/sqlite-amalgamation-3460100/sqlite3.c -o sqlite3.o \
 && gcc -std=c11 -O2 -Wall -Wno-unused-function -Wno-unused-variable -D_GNU_SOURCE \
      -Ic -Ithird_party/sqlite-amalgamation-3460100 \
      c/json.c c/sha256.c c/util.c c/db.c c/http.c c/auth.c c/profile.c c/upgrade.c c/catalog.c \
      c/question.c c/exam.c c/attempt.c c/dash.c c/ops.c c/seed.c c/routes.c c/main.c \
      sqlite3.o -o online_exam -lpthread -ldl -lm \
 && rm -rf c third_party sqlite3.o \
 && apt-get purge -y gcc libc6-dev \
 && apt-get autoremove -y \
 && rm -rf /var/lib/apt/lists/*
ENV PORT=8080
EXPOSE 8080
CMD ["/app/online_exam"]
