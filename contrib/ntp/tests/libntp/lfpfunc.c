#include "config.h"

#include "ntp_stdlib.h"
#include "ntp_calendar.h"
#include "ntp_fp.h"

#include "unity.h"

#include <float.h>
#include <math.h>


//replaced TEST_ASSERT_EQUAL_MEMORY(&a,&b,sizeof(a)) with TEST_ASSERT_EQUAL_l_fp(a,b). It's safer this way, because structs can be compared even if they aren't initiated with memset (due to padding bytes)
#define TEST_ASSERT_EQUAL_l_fp(a, b) { \
    TEST_ASSERT_EQUAL_MESSAGE(a.l_i, b.l_i, "Field l_i"); \
    TEST_ASSERT_EQUAL_UINT_MESSAGE(a.l_uf, b.l_uf, "Field l_uf");	\
}

typedef struct  {
	uint32_t h, l;
} lfp_hl;


static int cmp_work(u_int32 a[3], u_int32 b[3]);

/*
//----------------------------------------------------------------------
// OO-wrapper for 'l_fp'
//----------------------------------------------------------------------


	~LFP();
	LFP();
	LFP(const LFP& rhs);
	LFP(int32 i, u_int32 f);

	LFP  operator+ (const LFP &rhs) const;
	LFP& operator+=(const LFP &rhs);

	LFP  operator- (const LFP &rhs) const;
	LFP& operator-=(const LFP &rhs);

	LFP& operator=(const LFP &rhs);
	LFP  operator-() const;

	bool operator==(const LFP &rhs) const;

	LFP  neg() const;
	LFP  abs() const;
	int  signum() const;

	

	int  ucmp(const LFP & rhs) const;
	int  scmp(const LFP & rhs) const;
	
	std::string   toString() const;
	std::ostream& toStream(std::ostream &oo) const;
	
	operator double() const;
	explicit LFP(double);
	

	LFP(const l_fp &rhs);

	static int cmp_work(u_int32 a[3], u_int32 b[3]);
	
	l_fp _v;

	
static std::ostream& operator<<(std::ostream &oo, const LFP& rhs)
{
	return rhs.toStream(oo);
}
*/
//----------------------------------------------------------------------
// reference comparision
// This is implementad as a full signed MP-subtract in 3 limbs, where
// the operands are zero or sign extended before the subtraction is
// executed.
//----------------------------------------------------------------------
int  l_fp_scmp(const l_fp first, const l_fp second)
{
	u_int32 a[3], b[3];
	
	const l_fp op1 = first;
	const l_fp op2 = second;
	//const l_fp &op1(_v), &op2(rhs._v);
	
	a[0] = op1.l_uf; a[1] = op1.l_ui; a[2] = 0;
	b[0] = op2.l_uf; b[1] = op2.l_ui; b[2] = 0;

	a[2] -= (op1.l_i < 0);
	b[2] -= (op2.l_i < 0);

	return cmp_work(a,b);
}

int  l_fp_ucmp(const l_fp first, l_fp second )
{
	u_int32 a[3], b[3];
	const l_fp op1 = first; 
	const l_fp op2 = second;
	
	a[0] = op1.l_uf; a[1] = op1.l_ui; a[2] = 0;
	b[0] = op2.l_uf; b[1] = op2.l_ui; b[2] = 0;

	return cmp_work(a,b);
}


//maybe rename it to lf_cmp_work ???
int cmp_work(u_int32 a[3], u_int32 b[3])
{
	u_int32 cy, idx, tmp;
	for (cy = idx = 0; idx < 3; ++idx) {
		tmp = a[idx]; cy  = (a[idx] -=   cy  ) > tmp;
		tmp = a[idx]; cy |= (a[idx] -= b[idx]) > tmp;
	}
	if (a[2])
		return -1;
	return a[0] || a[1];
}


//----------------------------------------------------------------------
// imlementation of the LFP stuff
// This should be easy enough...
//----------------------------------------------------------------------



l_fp l_fp_init(int32 i, u_int32 f)
{
	l_fp temp;
	temp.l_i  = i;
	temp.l_uf = f;

	return temp;
}



l_fp l_fp_add(const l_fp first, const l_fp second)
{
	l_fp temp;
	temp = first;
	L_ADD(&temp, &second);
	return temp;
}

l_fp l_fp_subtract(const l_fp first, const l_fp second)
{
	l_fp temp;
	temp = first;
	L_SUB(&temp, &second);
	
	return temp;
}

l_fp l_fp_negate(const l_fp first)
{
	l_fp temp;
	temp = first; //is this line really necessary?
	L_NEG(&temp);
	
	return temp;
}

l_fp l_fp_abs(const l_fp first)
{
	l_fp temp = first;
	if (L_ISNEG(&temp))
		L_NEG(&temp);
	return temp;
}

int l_fp_signum(const l_fp first)
{
	if (first.l_ui & 0x80000000u)
		return -1;
	return (first.l_ui || first.l_uf);
}

double l_fp_convert_to_double(const l_fp first)
{
	double res;
	LFPTOD(&first, res);
	return res;
}

l_fp l_fp_init_from_double( double rhs)
{
	l_fp temp;
	DTOLFP(rhs, &temp);
	return temp;
}



void l_fp_swap(l_fp * first, l_fp *second){
	l_fp temp = *second;

	*second = *first;
	*first = temp;

}


/*
LFP::LFP()
{
	_v.l_ui = 0;
	_v.l_uf = 0;
}



std::string
LFP::toString() const
{
	std::ostringstream oss;
	toStream(oss);
	return oss.str();
}

std::ostream&
LFP::toStream(std::ostream &os) const
{
	return os
	    << mfptoa(_v.l_ui, _v.l_uf, 9)
	    << " [$" << std::setw(8) << std::setfill('0') << std::hex << _v.l_ui
	    <<  ':'  << std::setw(8) << std::setfill('0') << std::hex << _v.l_uf
	    << ']';
}

bool LFP::operator==(const LFP &rhs) const
{
	return L_ISEQU(&_v, &rhs._v);
}



*/

//----------------------------------------------------------------------
// testing the relational macros works better with proper predicate
// formatting functions; it slows down the tests a bit, but makes for
// readable failure messages.
//----------------------------------------------------------------------


typedef int bool; //typedef enum { FALSE, TRUE } boolean; -> can't use this because TRUE and FALSE are already defined


bool l_isgt (const l_fp first, const l_fp second)
	{ return L_ISGT(&first, &second); }
bool l_isgtu(const l_fp first, const l_fp second)
	{ return L_ISGTU(&first, &second); }
bool l_ishis(const l_fp first, const l_fp second)
	{ return L_ISHIS(&first, &second); }
bool l_isgeq(const l_fp first, const l_fp second)
	{ return L_ISGEQ(&first, &second); }
bool l_isequ(const l_fp first, const l_fp second)
	{ return L_ISEQU(&first, &second); }


//----------------------------------------------------------------------
// test data table for add/sub and compare
//----------------------------------------------------------------------


static const lfp_hl addsub_tab[][3] = {
	// trivial idendity:
	{{0 ,0         }, { 0,0         }, { 0,0}},
	// with carry from fraction and sign change:
	{{-1,0x80000000}, { 0,0x80000000}, { 0,0}},
	// without carry from fraction
	{{ 1,0x40000000}, { 1,0x40000000}, { 2,0x80000000}},
	// with carry from fraction:
	{{ 1,0xC0000000}, { 1,0xC0000000}, { 3,0x80000000}},
	// with carry from fraction and sign change:
	{{0x7FFFFFFF, 0x7FFFFFFF}, {0x7FFFFFFF,0x7FFFFFFF}, {0xFFFFFFFE,0xFFFFFFFE}},
	// two tests w/o carry (used for l_fp<-->double):
	{{0x55555555,0xAAAAAAAA}, {0x11111111,0x11111111}, {0x66666666,0xBBBBBBBB}},
	{{0x55555555,0x55555555}, {0x11111111,0x11111111}, {0x66666666,0x66666666}},
	// wide-range test, triggers compare trouble
	{{0x80000000,0x00000001}, {0xFFFFFFFF,0xFFFFFFFE}, {0x7FFFFFFF,0xFFFFFFFF}}
};
static const size_t addsub_cnt = (sizeof(addsub_tab)/sizeof(addsub_tab[0]));
static const size_t addsub_tot = (sizeof(addsub_tab)/sizeof(addsub_tab[0][0]));



//----------------------------------------------------------------------
// epsilon estimation for the precision of a conversion double --> l_fp
//
// The error estimation limit is as follows:
//  * The 'l_fp' fixed point fraction has 32 bits precision, so we allow
//    for the LSB to toggle by clamping the epsilon to be at least 2^(-31)
//
//  * The double mantissa has a precsion 54 bits, so the other minimum is
//    dval * (2^(-53))
//
//  The maximum of those two boundaries is used for the check.
//
// Note: once there are more than 54 bits between the highest and lowest
// '1'-bit of the l_fp value, the roundtrip *will* create truncation
// errors. This is an inherent property caused by the 54-bit mantissa of
// the 'double' type.
double eps(double d)
{
	return fmax(ldexp(1.0, -31), ldexp(fabs(d), -53)); //max<double>
}



//----------------------------------------------------------------------
// test addition
//----------------------------------------------------------------------
void test_AdditionLR() {
	
	size_t idx=0;
	for (idx=0; idx < addsub_cnt; ++idx) {


		l_fp op1 = l_fp_init(addsub_tab[idx][0].h, addsub_tab[idx][0].l);
		//LFP op1(addsub_tab[idx][0].h, addsub_tab[idx][0].l);
		l_fp op2 = l_fp_init(addsub_tab[idx][1].h, addsub_tab[idx][1].l);
		//LFP exp(addsub_tab[idx][2].h, addsub_tab[idx][2].l);
		l_fp exp = l_fp_init(addsub_tab[idx][2].h, addsub_tab[idx][2].l);
		//LFP res(op1 + op2);
		l_fp res = l_fp_add(op1,op2);		

		TEST_ASSERT_EQUAL_l_fp(exp,res);
		//TEST_ASSERT_EQUAL_MEMORY(&exp, &res,sizeof(exp));
	}	
}

void test_AdditionRL() {

	size_t idx=0;
	for (idx=0; idx < addsub_cnt; ++idx) {
		l_fp op2 = l_fp_init(addsub_tab[idx][0].h, addsub_tab[idx][0].l);
		l_fp op1 = l_fp_init(addsub_tab[idx][1].h, addsub_tab[idx][1].l);
		l_fp exp = l_fp_init(addsub_tab[idx][2].h, addsub_tab[idx][2].l);
		l_fp res = l_fp_add(op1,op2);

		TEST_ASSERT_EQUAL_l_fp(exp,res);
		//TEST_ASSERT_EQUAL_MEMORY(&exp, &res,sizeof(exp));
	}	
}



//----------------------------------------------------------------------
// test subtraction
//----------------------------------------------------------------------
void test_SubtractionLR() {

	size_t idx=0;
	for (idx=0; idx < addsub_cnt; ++idx) {
		l_fp op2 = l_fp_init(addsub_tab[idx][0].h, addsub_tab[idx][0].l);
		l_fp exp = l_fp_init(addsub_tab[idx][1].h, addsub_tab[idx][1].l);
		l_fp op1 = l_fp_init(addsub_tab[idx][2].h, addsub_tab[idx][2].l);
		l_fp res = l_fp_subtract(op1,op2);
		//LFP res(op1 - op2);
		
		TEST_ASSERT_EQUAL_l_fp(exp,res);		
		//TEST_ASSERT_EQUAL_MEMORY(&exp, &res,sizeof(exp));
	}	
}

void test_SubtractionRL() {

	size_t idx=0;
	for (idx=0; idx < addsub_cnt; ++idx) {
		l_fp exp = l_fp_init(addsub_tab[idx][0].h, addsub_tab[idx][0].l);
		l_fp op2 = l_fp_init(addsub_tab[idx][1].h, addsub_tab[idx][1].l);
		l_fp op1 = l_fp_init(addsub_tab[idx][2].h, addsub_tab[idx][2].l);
		l_fp res = l_fp_subtract(op1,op2);

		TEST_ASSERT_EQUAL_l_fp(exp,res);
		//TEST_ASSERT_EQUAL_MEMORY(&exp, &res,sizeof(exp));
	}	
}

//----------------------------------------------------------------------
// test negation
//----------------------------------------------------------------------

void test_Negation() {

	size_t idx=0;
	for (idx=0; idx < addsub_cnt; ++idx) {
		l_fp op1 = l_fp_init(addsub_tab[idx][0].h, addsub_tab[idx][0].l);
		l_fp op2 = l_fp_negate(op1);
		l_fp sum = l_fp_add(op1, op2);
		
		l_fp zero = l_fp_init(0,0);

		TEST_ASSERT_EQUAL_l_fp(zero,sum);
		//TEST_ASSERT_EQUAL_MEMORY(&zero, &sum,sizeof(sum));
	
	}	
}



//----------------------------------------------------------------------
// test absolute value
//----------------------------------------------------------------------
void test_Absolute() {
	size_t idx=0;
	for (idx=0; idx < addsub_cnt; ++idx) {
		l_fp op1 = l_fp_init(addsub_tab[idx][0].h, addsub_tab[idx][0].l);
		l_fp op2 = l_fp_abs(op1);

		TEST_ASSERT_TRUE(l_fp_signum(op2) >= 0);		

		if (l_fp_signum(op1) >= 0)
			op1 = l_fp_subtract(op1,op2); //op1 -= op2;
			
		else
			op1 = l_fp_add(op1,op2);
		
		l_fp zero = l_fp_init(0,0);
		
		TEST_ASSERT_EQUAL_l_fp(zero,op1);
		//TEST_ASSERT_EQUAL_MEMORY(&zero, &op1,sizeof(op1));
	}

	// There is one special case we have to check: the minimum
	// value cannot be negated, or, to be more precise, the
	// negation reproduces the original pattern.
	l_fp minVal = l_fp_init(0x80000000, 0x00000000);
	l_fp minAbs = l_fp_abs(minVal);
	TEST_ASSERT_EQUAL(-1, l_fp_signum(minVal));

	TEST_ASSERT_EQUAL_l_fp(minVal,minAbs);
	//TEST_ASSERT_EQUAL_MEMORY(&minVal, &minAbs,sizeof(minAbs));
}


//----------------------------------------------------------------------
// fp -> double -> fp rountrip test
//----------------------------------------------------------------------
void test_FDF_RoundTrip() {
	// since a l_fp has 64 bits in it's mantissa and a double has
	// only 54 bits available (including the hidden '1') we have to
	// make a few concessions on the roundtrip precision. The 'eps()'
	// function makes an educated guess about the avilable precision
	// and checks the difference in the two 'l_fp' values against
	// that limit.
	size_t idx=0;
	for (idx=0; idx < addsub_cnt; ++idx) {
		l_fp op1 = l_fp_init(addsub_tab[idx][0].h, addsub_tab[idx][0].l);
		double op2 = l_fp_convert_to_double(op1);
		l_fp op3 = l_fp_init_from_double(op2); 

		// for manual checks only:
		// std::cout << std::setprecision(16) << op2 << std::endl;

		l_fp temp = l_fp_subtract(op1,op3);
		double d = l_fp_convert_to_double(temp);
 		TEST_ASSERT_DOUBLE_WITHIN(eps(op2),0.0, fabs(d)); //delta,epected,actual
		  		
		//ASSERT_LE(fabs(op1-op3), eps(op2)); //unity has no equivalent of LE!!!
		//you could use TEST_ASSERT_TRUE(IsLE(fabs(op1-op3), eps(op2)));
	}	
}


//----------------------------------------------------------------------
// test the compare stuff
//
// This uses the local compare and checks if the operations using the
// macros in 'ntp_fp.h' produce mathing results.
// ----------------------------------------------------------------------
void test_SignedRelOps() {
	//const lfp_hl * tv(&addsub_tab[0][0]);
	const lfp_hl * tv = (&addsub_tab[0][0]);
	size_t lc ;
	for (lc=addsub_tot-1; lc; --lc,++tv) {
		l_fp op1 = l_fp_init(tv[0].h,tv[0].l);
		l_fp op2 = l_fp_init(tv[1].h,tv[1].l);
		//int cmp(op1.scmp(op2));
		int cmp = l_fp_scmp(op1,op2);		
		
		switch (cmp) {
		case -1:
			//printf("op1:%d %d, op2:%d %d\n",op1.l_uf,op1.l_ui,op2.l_uf,op2.l_ui);
			//std::swap(op1, op2);
			l_fp_swap(&op1,&op2);
			//printf("op1:%d %d, op2:%d %d\n",op1.l_uf,op1.l_ui,op2.l_uf,op2.l_ui);
		case 1:
			TEST_ASSERT_TRUE (l_isgt(op1,op2));
			TEST_ASSERT_FALSE(l_isgt(op2,op1));

			TEST_ASSERT_TRUE (l_isgeq(op1,op2));
			TEST_ASSERT_FALSE(l_isgeq(op2,op1));

			TEST_ASSERT_FALSE(l_isequ(op1,op2));
			TEST_ASSERT_FALSE(l_isequ(op2,op1));
			break;
		case 0:
			TEST_ASSERT_FALSE(l_isgt(op1,op2));
			TEST_ASSERT_FALSE(l_isgt(op2,op1));

			TEST_ASSERT_TRUE (l_isgeq(op1,op2));
			TEST_ASSERT_TRUE (l_isgeq(op2,op1));

			TEST_ASSERT_TRUE (l_isequ(op1,op2));
			TEST_ASSERT_TRUE (l_isequ(op2,op1));
			break;
		default:
			TEST_FAIL_MESSAGE("unexpected UCMP result: " );	
			//TEST_ASSERT_FAIL() << "unexpected SCMP result: " << cmp;
		}
	}
}

void test_UnsignedRelOps() {
	const lfp_hl * tv =(&addsub_tab[0][0]);	
	size_t lc;
	for (lc=addsub_tot-1; lc; --lc,++tv) {
		l_fp op1 = l_fp_init(tv[0].h,tv[0].l);
		l_fp op2 = l_fp_init(tv[1].h,tv[1].l);
		int cmp = l_fp_ucmp(op1,op2);

		switch (cmp) {
		case -1:
			//printf("op1:%d %d, op2:%d %d\n",op1.l_uf,op1.l_ui,op2.l_uf,op2.l_ui);
			l_fp_swap(&op1, &op2);
			//printf("op1:%d %d, op2:%d %d\n",op1.l_uf,op1.l_ui,op2.l_uf,op2.l_ui);
		case 1:
			TEST_ASSERT_TRUE (l_isgtu(op1,op2));
			TEST_ASSERT_FALSE(l_isgtu(op2,op1));

			TEST_ASSERT_TRUE (l_ishis(op1,op2));
			TEST_ASSERT_FALSE(l_ishis(op2,op1));
			break;
		case 0:
			TEST_ASSERT_FALSE(l_isgtu(op1,op2));
			TEST_ASSERT_FALSE(l_isgtu(op2,op1));

			TEST_ASSERT_TRUE (l_ishis(op1,op2));
			TEST_ASSERT_TRUE (l_ishis(op2,op1));
			break;
		default:
			TEST_FAIL_MESSAGE("unexpected UCMP result: " );	
			//FAIL() << "unexpected UCMP result: " << cmp;
		}
	}
}
/*
*/

//----------------------------------------------------------------------
// that's all folks... but feel free to add things!
//----------------------------------------------------------------------
