import datetime as dt
import importlib.util
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location("companion", Path(__file__).resolve().parents[1] / "tools/pet_companion.py")
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)

HEADER = "source,model,project,hostname,bucketStart,totalTokens,inputTokens,outputTokens\n"


class CompanionTests(unittest.TestCase):
    def test_timezone_and_dedup(self):
        row = "code,m,p,h,2026-09-06T16:00:00Z,100,30,80\n"
        totals = c.aggregate_csv(HEADER + row + row)
        self.assertEqual(totals, {dt.date(2026, 9, 7): 110})

    def test_invalid_data(self):
        for content in ("", "NA", HEADER + "code,m,p,h,2026-09-07,-1,0,0\n",
                        HEADER + "code,m,p,h,2026-09-07T00:00:00Z,NA,0,0\n"):
            with self.assertRaises(ValueError):
                c.aggregate_csv(content)
        row = "code,m,p,h,2026-09-07T00:00:00Z,100,0,0\n"
        with self.assertRaises(ValueError):
            c.aggregate_csv(HEADER + row + row.replace(",100,", ",200,"))

    def test_food_and_cap(self):
        self.assertEqual([c.food(n, 100) for n in (0, 1, 20, 21, 40, 80, 81, 100, 10**12)],
                         [0, 1, 1, 2, 2, 4, 5, 5, 5])

    def test_median_excludes_today_and_future(self):
        today = dt.date(2026, 9, 7)
        totals = {today - dt.timedelta(days=1): 100, today - dt.timedelta(days=2): 200,
                  today: 9000, today + dt.timedelta(days=1): 99999}
        self.assertEqual(c.choose_goal(totals, today), 150)

    def test_snapshot_future_zero_and_no_private_fields(self):
        today = dt.date(2026, 9, 7)
        line = c.make_snapshot({today: 20, today + dt.timedelta(days=1): 900}, today, 100)
        self.assertEqual(line, "PET2 SYNC 20260907 20 100 0000001000000000000000000000000")

    def test_february(self):
        line = c.make_snapshot({}, dt.date(2026, 2, 28), 100)
        self.assertTrue(line.endswith("0" * 31))


if __name__ == "__main__":
    unittest.main()
