#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    printf("Launching smallish...\n");

    // Replace THIS process with your smallish shell
    execl("./smallish", "./smallish", (char *)NULL);

    // If exec fails:
    perror("execl");
    return 1;
}

