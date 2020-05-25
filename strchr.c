#include<stdio.h>
#include<string.h>
#include<stdlib.h>
int main (void) {
    char *s = malloc(20);
    strcpy(s,"oneline");
    printf("%s\n",s);
    strcpy(s,"two");
    printf("%s\n",s);
    strcpy(s,"thirdline");
    printf("%s\n",s);
    free(s);
    return 0;
}
