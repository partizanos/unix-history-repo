/* AUTOGENERATED FILE. DO NOT EDIT. */

//=======Test Runner Used To Run Each Test Below=====
#define RUN_TEST(TestFunc, TestLineNum) \
{ \
  Unity.CurrentTestName = #TestFunc; \
  Unity.CurrentTestLineNumber = TestLineNum; \
  Unity.NumberOfTests++; \
  if (TEST_PROTECT()) \
  { \
      setUp(); \
      TestFunc(); \
  } \
  if (TEST_PROTECT() && !TEST_IS_IGNORED) \
  { \
    tearDown(); \
  } \
  UnityConcludeTest(); \
}

//=======Automagically Detected Files To Include=====
#include "unity.h"
#include <setjmp.h>
#include <stdio.h>

//=======External Functions This Runner Calls=====
extern void setUp(void);
extern void tearDown(void);
void resetTest(void);
extern void test_IPv4AddressOnly(void);
extern void test_IPv4AddressWithPort(void);
extern void test_IPv6AddressOnly(void);
extern void test_IPv6AddressWithPort(void);
extern void test_IllegalAddress(void);
extern void test_IllegalCharInPort(void);


//=======Test Reset Option=====
void resetTest()
{
  tearDown();
  setUp();
}

char *progname;


//=======MAIN=====
int main(int argc, char *argv[])
{
  progname = argv[0];
  Unity.TestFile = "decodenetnum.c";
  UnityBegin("decodenetnum.c");
  RUN_TEST(test_IPv4AddressOnly, 9);
  RUN_TEST(test_IPv4AddressWithPort, 22);
  RUN_TEST(test_IPv6AddressOnly, 35);
  RUN_TEST(test_IPv6AddressWithPort, 55);
  RUN_TEST(test_IllegalAddress, 75);
  RUN_TEST(test_IllegalCharInPort, 82);

  return (UnityEnd());
}
