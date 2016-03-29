////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 - 2016, Paul Beckingham, Federico Hernandez.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// http://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#include <cmake.h>
#include <timew.h>
#include <Duration.h>
#include <sstream>

////////////////////////////////////////////////////////////////////////////////
Color tagColor (const Rules& rules, const std::string& tag)
{
  Color c;
  std::string name = std::string ("tag.") + tag + ".color";
  if (rules.has (name))
    c = Color (rules.get (name));

  return c;
}

////////////////////////////////////////////////////////////////////////////////
std::string intervalSummarize (const Rules& rules, const Interval& interval)
{
  std::stringstream out;

  if (interval.isStarted ())
  {
    if (interval.isEnded ())
    {
      Duration dur (Datetime (interval.end ()) - Datetime (interval.start ()));
      out << "Recorded interval from "
          << interval.start ().toISOLocalExtended ()
          << " to "
          << interval.end ().toISOLocalExtended ()
          << " ("
          << dur.format ()
          << ")";
    }
    else
    {
      Duration dur (Datetime () - interval.start ());
      out << "Active interval since "
          << interval.start ().toISOLocalExtended ();

      if (dur.toTime_t () > 10)
        out << " ("
            << dur.format ()
            << ")";
    }

    // Colorize tags.
    auto tags = interval.tags ();
    if (tags.size ())
    {
      out << ", tagged:";
      for (auto& tag : tags)
        out << ' ' << tagColor (rules, tag).colorize (quoteIfNeeded (tag));
    }

    out << "\n";
  }

  return out.str ();
}

////////////////////////////////////////////////////////////////////////////////