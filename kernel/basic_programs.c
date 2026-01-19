// Example BASIC programs for VOS
// These are designed to work with uBASIC's limited feature set:
// - PRINT, IF/THEN/ELSE, FOR/NEXT, GOTO, GOSUB/RETURN, LET, END
// - Integer variables a-z only
// - Operators: + - * / % & |
// - Comparisons: < > =

#include "basic_programs.h"

// Program 1: Fibonacci Sequence
const char* basic_program_1 =
    "10 print \"=== Fibonacci Sequence ===\"\n"
    "20 print \"First 20 Fibonacci numbers:\"\n"
    "30 let a = 0\n"
    "40 let b = 1\n"
    "50 for i = 1 to 20\n"
    "60 print a\n"
    "70 let c = a + b\n"
    "80 let a = b\n"
    "90 let b = c\n"
    "100 next i\n"
    "110 print \"Done!\"\n"
    "120 end\n";

// Program 2: Prime Number Finder
const char* basic_program_2 =
    "10 print \"=== Prime Numbers ===\"\n"
    "20 print \"Primes from 2 to 100:\"\n"
    "30 for n = 2 to 100\n"
    "40 let p = 1\n"
    "50 for d = 2 to n - 1\n"
    "60 let r = n % d\n"
    "70 if r = 0 then let p = 0\n"
    "80 next d\n"
    "90 if p = 1 then print n\n"
    "100 next n\n"
    "110 print \"Done!\"\n"
    "120 end\n";

// Program 3: Multiplication Table
const char* basic_program_3 =
    "10 print \"=== Multiplication Table ===\"\n"
    "20 print \"Table from 1 to 9:\"\n"
    "30 for i = 1 to 9\n"
    "40 for j = 1 to 9\n"
    "50 let r = i * j\n"
    "60 print r,\n"
    "70 next j\n"
    "80 print \"\"\n"
    "90 next i\n"
    "100 print \"Done!\"\n"
    "110 end\n";

// Program 4: Factorial Calculator
const char* basic_program_4 =
    "10 print \"=== Factorials ===\"\n"
    "20 print \"Calculating 1! to 12!:\"\n"
    "30 for n = 1 to 12\n"
    "40 let f = 1\n"
    "50 for i = 1 to n\n"
    "60 let f = f * i\n"
    "70 next i\n"
    "80 print n, \"! =\", f\n"
    "90 next n\n"
    "100 print \"Done!\"\n"
    "110 end\n";

// Program 5: Number Pyramid
const char* basic_program_5 =
    "10 print \"=== Number Pyramid ===\"\n"
    "20 let h = 9\n"
    "30 for i = 1 to h\n"
    "40 for s = 1 to h - i\n"
    "50 print \" \",\n"
    "60 next s\n"
    "70 for j = 1 to i\n"
    "80 print j,\n"
    "90 next j\n"
    "100 for k = i - 1 to 1\n"
    "110 print k,\n"
    "120 next k\n"
    "130 print \"\"\n"
    "140 next i\n"
    "150 print \"Done!\"\n"
    "160 end\n";

// Program 6: Powers of 2
const char* basic_program_6 =
    "10 print \"=== Powers of 2 ===\"\n"
    "20 print \"2^0 to 2^20:\"\n"
    "30 let p = 1\n"
    "40 for n = 0 to 20\n"
    "50 print \"2^\", n, \"=\", p\n"
    "60 let p = p * 2\n"
    "70 next n\n"
    "80 print \"Done!\"\n"
    "90 end\n";

// Program 7: GCD (Greatest Common Divisor) using Euclidean Algorithm
const char* basic_program_7 =
    "10 print \"=== GCD Calculator ===\"\n"
    "20 print \"Finding GCD of 252 and 105:\"\n"
    "30 let a = 252\n"
    "40 let b = 105\n"
    "50 print \"a =\", a, \" b =\", b\n"
    "60 if b = 0 then goto 110\n"
    "70 let t = b\n"
    "80 let b = a % b\n"
    "90 let a = t\n"
    "100 goto 60\n"
    "110 print \"GCD =\", a\n"
    "120 print \"\"\n"
    "130 print \"Finding GCD of 48 and 18:\"\n"
    "140 let a = 48\n"
    "150 let b = 18\n"
    "160 print \"a =\", a, \" b =\", b\n"
    "170 if b = 0 then goto 220\n"
    "180 let t = b\n"
    "190 let b = a % b\n"
    "200 let a = t\n"
    "210 goto 170\n"
    "220 print \"GCD =\", a\n"
    "230 print \"Done!\"\n"
    "240 end\n";

// Program 8: Sum of Series
const char* basic_program_8 =
    "10 print \"=== Mathematical Series ===\"\n"
    "20 print \"\"\n"
    "30 print \"Sum 1+2+3+...+100:\"\n"
    "40 let s = 0\n"
    "50 for i = 1 to 100\n"
    "60 let s = s + i\n"
    "70 next i\n"
    "80 print \"Sum =\", s\n"
    "90 print \"\"\n"
    "100 print \"Sum of squares 1+4+9+...+100:\"\n"
    "110 let s = 0\n"
    "120 for i = 1 to 10\n"
    "130 let s = s + i * i\n"
    "140 next i\n"
    "150 print \"Sum =\", s\n"
    "160 print \"\"\n"
    "170 print \"Sum of cubes 1+8+27+...+1000:\"\n"
    "180 let s = 0\n"
    "190 for i = 1 to 10\n"
    "200 let s = s + i * i * i\n"
    "210 next i\n"
    "220 print \"Sum =\", s\n"
    "230 print \"Done!\"\n"
    "240 end\n";

// Program 9: Triangle Patterns
const char* basic_program_9 =
    "10 print \"=== Triangle Patterns ===\"\n"
    "20 print \"\"\n"
    "30 print \"Right Triangle:\"\n"
    "40 for i = 1 to 8\n"
    "50 for j = 1 to i\n"
    "60 print \"*\",\n"
    "70 next j\n"
    "80 print \"\"\n"
    "90 next i\n"
    "100 print \"\"\n"
    "110 print \"Inverted Triangle:\"\n"
    "120 for i = 8 to 1\n"
    "130 for j = 1 to i\n"
    "140 print \"*\",\n"
    "150 next j\n"
    "160 print \"\"\n"
    "170 next i\n"
    "180 print \"Done!\"\n"
    "190 end\n";

// Program 10: Collatz Conjecture (3n+1 problem)
const char* basic_program_10 =
    "10 print \"=== Collatz Conjecture ===\"\n"
    "20 print \"Starting from 27:\"\n"
    "30 let n = 27\n"
    "40 let c = 0\n"
    "50 print n\n"
    "60 if n = 1 then goto 120\n"
    "70 let c = c + 1\n"
    "80 let r = n % 2\n"
    "90 if r = 0 then let n = n / 2\n"
    "100 if r = 1 then let n = 3 * n + 1\n"
    "110 goto 50\n"
    "120 print \"Reached 1 in\", c, \"steps\"\n"
    "130 print \"\"\n"
    "140 print \"Starting from 97:\"\n"
    "150 let n = 97\n"
    "160 let c = 0\n"
    "170 print n\n"
    "180 if n = 1 then goto 240\n"
    "190 let c = c + 1\n"
    "200 let r = n % 2\n"
    "210 if r = 0 then let n = n / 2\n"
    "220 if r = 1 then let n = 3 * n + 1\n"
    "230 goto 170\n"
    "240 print \"Reached 1 in\", c, \"steps\"\n"
    "250 print \"Done!\"\n"
    "260 end\n";

// Program names and descriptions
const char* basic_program_names[] = {
    "Fibonacci Sequence",
    "Prime Number Finder",
    "Multiplication Table",
    "Factorial Calculator",
    "Number Pyramid",
    "Powers of 2",
    "GCD Calculator",
    "Sum of Series",
    "Triangle Patterns",
    "Collatz Conjecture"
};

const char* basic_program_descriptions[] = {
    "Calculate first 20 Fibonacci numbers",
    "Find all primes from 2 to 100",
    "Display 9x9 multiplication table",
    "Calculate factorials from 1! to 12!",
    "Draw a number pyramid pattern",
    "Calculate powers of 2 up to 2^20",
    "Find GCD using Euclidean algorithm",
    "Sum of integers, squares, and cubes",
    "Draw triangle patterns with asterisks",
    "Demonstrate the 3n+1 conjecture"
};

// Get program by index (1-10)
const char* basic_get_program(int index) {
    switch (index) {
        case 1: return basic_program_1;
        case 2: return basic_program_2;
        case 3: return basic_program_3;
        case 4: return basic_program_4;
        case 5: return basic_program_5;
        case 6: return basic_program_6;
        case 7: return basic_program_7;
        case 8: return basic_program_8;
        case 9: return basic_program_9;
        case 10: return basic_program_10;
        default: return 0;
    }
}

const char* basic_get_program_name(int index) {
    if (index >= 1 && index <= 10) {
        return basic_program_names[index - 1];
    }
    return 0;
}

const char* basic_get_program_description(int index) {
    if (index >= 1 && index <= 10) {
        return basic_program_descriptions[index - 1];
    }
    return 0;
}
