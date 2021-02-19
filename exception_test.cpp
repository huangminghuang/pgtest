#include <pqxx/pqxx>
#include <stdio.h>

int main() {

   try {
      throw pqxx::unique_violation("","");
   } catch (const pqxx::integrity_constraint_violation&) {
      printf("exception catched");
   }

   return 0;
}