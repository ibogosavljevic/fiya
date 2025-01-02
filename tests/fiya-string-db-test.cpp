#include "fiya-string-db.h"
#include <cassert>

using namespace fiya;

int main(int argc, char ** argv) {
    string_db_t string_db(1);

    size_t idx_dog = string_db.push_back("dog");
    size_t idx_cat = string_db.push_back("cat");
    size_t idx_dog1 = string_db.push_back("dog");
    assert(idx_dog == idx_dog1);
    assert(strcmp(string_db.get(idx_dog), "dog") == 0);
    assert(strcmp(string_db.get(idx_cat), "cat") == 0);

}