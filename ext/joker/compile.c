#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include "compile.h"


static void
jkwarn(message)
    const char *  message;
{
    printf("%s\n", message);
    // TODO
}


static int
hash(cchar)
    const char  cchar;
{
    switch (cchar) {
        case '\\':
            return 0;
        case '[':
            return 1;
        case ']':
            return 2;
        case '*':
            return 3;
        case '?':
            return 4;
        default:
            return 5;
    }
}


static void *
Array_enlarge(array, entry_size, old_size)
    void **   array;
    int       entry_size;
    long int  old_size;
{
    void *    new_array;

    new_array = malloc(entry_size * (old_size + 1));
    if (*array != 0) {
        memcpy(new_array, *array, entry_size * old_size);
        free(*array);
    }
    *array = new_array;
    return new_array + old_size;
}


static void
push_fixed(cchar, wildcard)
    const char  cchar;
    Wildcard *  wildcard;
{
    char *      new_char;
    Wildpart *  part;

    switch (wildcard->last) {
        case Fixed:  // add to current string
            part        = wildcard->parts + wildcard->length - 1;
            new_char = Array_enlarge(&part->data, sizeof(char), part->length);
            part->length += 1;
            *(new_char - 1)  = cchar;
            *new_char        = '\0';
            break;
        default:     // add new array entry
            part = Array_enlarge(&wildcard->parts, sizeof(Wildpart), wildcard->length);
            wildcard->length += 1;
            part->type     = Fixed;
            part->length   = 1;
            part->data     = malloc(sizeof(char)*2);
            part->data[0]  = cchar;
            part->data[1]  = '\0';
            break;
    }
}


static void
push_wild(wildcard)
    Wildcard *  wildcard;
{
    Wildpart *  part;

    switch (wildcard->last) {
        case Kleene:  // transform *? --> *
            break;
        default:      // add new array entry
            part = Array_enlarge(&wildcard->parts, sizeof(Wildpart), wildcard->length);
            wildcard->length += 1;
            part->type = Wild;
            break;
    }
}


static void
push_kleene(wildcard)
    Wildcard *  wildcard;
{
    Wildpart *  part;

    switch (wildcard->last) {
        case Kleene:  // transform ** --> *
            break;
        case Wild:    // transform ?* --> *
            part = wildcard->parts + wildcard->length - 1;
            part->type = Kleene;
            break;
        default:      // add new array entry
            part = Array_enlarge(&wildcard->parts, sizeof(Wildpart), wildcard->length);
            wildcard->length += 1;
            part->type = Kleene;
            break;
    }
}


static void
push_group(cchar, wildcard)
    const char  cchar;
    Wildcard *  wildcard;
{
    char *      new_char;
    Wildpart *  part;

    switch (wildcard->last) {
        case Group:  // add to current group
            part      = wildcard->parts + wildcard->length - 1;
            new_char  = Array_enlarge(&part->data, sizeof(char), part->length);
            part->length += 1;
            *(new_char - 1)  = cchar;
            *new_char        = '\0';
            break;
        default:     // add new array entry
            part = Array_enlarge(&wildcard->parts, sizeof(Wildpart), wildcard->length);
            wildcard->length += 1;
            part->type     = Group;
            part->length   = 1;
            part->data     = malloc(sizeof(char)*2);
            part->data[0]  = cchar;
            part->data[1]  = '\0';
            break;
    }
}


static void
do_transition(transition, input, state, wildcard)
    const char  transition;
    const char  input;
    int *       state;
    Wildcard *  wildcard;
{
    switch (transition) {
        case 0:
            *state = 1;
            break;
        case 1:
            *state = 2;
            break;
        case 2:
            push_fixed(input, wildcard);
            jkwarn("Unescaped ]");
            break;
        case 3:
            push_kleene(wildcard);
            break;
        case 4:
            push_wild(wildcard);
            break;
        case 5:
            push_fixed(input, wildcard);
            break;
        case 6:
            *state = -1;
            break;
        case 7:
            *state = 0;
            push_fixed(input, wildcard);
            break;
        case 8:
            *state = 0;
            push_fixed('\\', wildcard);
            push_fixed(input, wildcard);
            break;
        case 9:
            *state = -1;
            push_fixed('\\', wildcard);
            break;
        case 10:
            *state = 3;
            break;
        case 11:
            push_group(input, wildcard);
            jkwarn("Unescaped [ in group");
            break;
        case 12:
            *state = 0;
            break;
        case 13:
            push_group(input, wildcard);
            break;
        case 14:
            *state = -1;
            jkwarn("Unfinished group");
            // throw ruby error
            break;
        case 15:
            *state = 2;
            push_group(input, wildcard);
            break;
        case 16:
            *state = 2;
            push_group('\\', wildcard);
            push_group(input, wildcard);
            break;
    }
}


void
Wildcard_compile(cstring, len, wildcard)
    const char *    cstring;
    const long int  len;
    Wildcard *      wildcard;
{
    // the table that maps (state x input) -> transition
    const char transition_table[4][7] = {
    //    \   [   ]   *   ? any EOS
        { 0,  1,  2,  3,  4,  5,  6},
        { 7,  7,  7,  7,  7,  8,  9},
        {10, 11, 12, 13, 13, 13, 14},
        {15, 15, 15, 16, 16, 16, 14}
    };
    int state = 0;

    long int  p;
    char      input;
    int       hashed;
    char      transition;

    for (p = 0; p < len; p++) {
        input = cstring[p];
        hashed = hash(input);
        transition = transition_table[state][hashed];
        do_transition(transition, input, &state, wildcard);
    }

    transition = transition_table[state][6];
    do_transition(transition, '\0', &state, wildcard);
}

