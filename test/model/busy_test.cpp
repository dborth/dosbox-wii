// Host test of MenuBusyPacer: when the "please wait" panel comes up and goes, and how its spinner turns.
#include "menubusypacer.h"
#include <cstdio>

static int checks = 0, failures = 0;
#define CHECK(c) do { checks++; if(!(c)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while(0)
#define CHECK_N(a, b) do { checks++; const long x = (long)(a), y = (long)(b); if(x != y) { failures++; printf("FAIL %s:%d  %s\n   got  %ld\n   want %ld\n", __FILE__, __LINE__, #a, x, y); } } while(0)

// work that is finished before the panel is due shows nothing at all
static void testQuickWork()
{
	MenuBusyPacer p;
	p.update(0, false);
	CHECK(!p.shown() && !p.over());
	p.update(120, false);
	CHECK(!p.shown() && !p.over());
	p.update(150, true);
	CHECK(!p.shown());
	CHECK(p.over());  // done, and never shown: nothing to wait for
	CHECK_N(p.phase(), 0);

	// found done only after the panel was due (a stalled frame): still not worth showing it for
	MenuBusyPacer late;
	late.update(5000, true);
	CHECK(!late.shown());
	CHECK(late.over());
}

// the panel comes up after the delay and the spinner turns one dot a step
static void testShowing()
{
	MenuBusyPacer p;
	p.update(MenuBusyPacer::DELAY_MS - 1, false);
	CHECK(!p.shown());
	p.update(MenuBusyPacer::DELAY_MS, false);
	CHECK(p.shown());
	CHECK_N(p.phase(), 0);  // the spinner starts at its head
	p.update(MenuBusyPacer::DELAY_MS + MenuBusyPacer::STEP_MS - 1, false);
	CHECK_N(p.phase(), 0);
	p.update(MenuBusyPacer::DELAY_MS + MenuBusyPacer::STEP_MS, false);
	CHECK_N(p.phase(), 1);
	p.update(MenuBusyPacer::DELAY_MS + 5 * MenuBusyPacer::STEP_MS + 3, false);
	CHECK_N(p.phase(), 5);
	CHECK(!p.over());

	// work that goes on for ever keeps the panel up, and the spinner turning
	p.update(10000, false);
	CHECK(p.shown() && !p.over());
	CHECK(p.phase() > 100);

	// once shown it stays shown: the delay is not applied again
	MenuBusyPacer q;
	q.update(500, false);
	CHECK(q.shown());
	q.update(600, false);
	CHECK(q.shown());
}

// once up, the panel stays long enough to read: it is not a flash
static void testMinimum()
{
	const uint32_t d = MenuBusyPacer::DELAY_MS, m = MenuBusyPacer::MIN_SHOWN_MS;

	MenuBusyPacer p;
	p.update(d, false);
	p.update(d + 50, true);  // the work finished just after the panel came up
	CHECK(p.shown() && !p.over());
	p.update(d + m - 1, true);
	CHECK(!p.over());
	p.update(d + m, true);
	CHECK(p.over());

	// work that took longer than that is over as soon as it is done
	MenuBusyPacer s;
	s.update(d, false);
	s.update(d + m + 200, false);
	s.update(d + m + 216, true);
	CHECK(s.over());

	// not done: never over, however long the panel has been up
	MenuBusyPacer n;
	n.update(d, false);
	n.update(d + 100 * m, false);
	CHECK(!n.over());
}

int main()
{
	testQuickWork();
	testShowing();
	testMinimum();

	if(failures)
		printf("busy tests: %d of %d checks FAILED\n", failures, checks);
	else
		printf("busy tests: %d checks passed\n", checks);
	return failures ? 1 : 0;
}
