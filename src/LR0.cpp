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
#include <LR0.h>
#include <Lexer.h>
#include <Table.h>
#include <Color.h>
#include <text.h>
#include <iostream>
#include <sstream>

////////////////////////////////////////////////////////////////////////////////
// Given a grammar:
// - Obtain the augmented grammar
// - Create the first state from the first augmented grammar rule
// - Expand the first state
// - Recursively create and expand all the other states
void LR0::initialize (const Grammar& grammar)
{
  // Save important grammar information.
  _augmented = grammar.augmented ();
  _rules     = grammar.rules ();
  _terminals = grammar.terminals ();

  if (_debug)
  {
    std::cout << "Augmented Grammar\n";
    auto count = 0;
    for (auto& item : _augmented)
    {
      std::cout << "  [" << count++ << "]";
      for (auto& term : item)
        std::cout << " " << term;
      std::cout << "\n";
    }

    std::cout << "\n";
    std::cout << "Rules\n";
    for (auto& r : _rules)
      std::cout << "  " << r << "\n";

    std::cout << "\n";
    std::cout << "Terminals\n";
    for (auto& t : _terminals)
      std::cout << "  " << t << "\n";

    std::cout << "\n";
  }

  // Determine the parser states.
  States states;
  initializeFirstState (states);
  closeState (states, 0);
  if (_debug)
    std::cout << states.dump () << "\n";

  // Construct the parser tables.
  createParseTable (states);
  if (_debug)
    std::cout << dump () << "\n";;
}

////////////////////////////////////////////////////////////////////////////////
// Collect a unique set of expected symbols from the closure. This is the set
// of symbols where '● X' appears in an item.
std::set <std::string> LR0::getExpectedSymbols (const Closure& closure) const
{
  std::set <std::string> expected;
  for (auto& item : closure)
    if (! item.done ())
      expected.insert (item.next ());

  return expected;
}

////////////////////////////////////////////////////////////////////////////////
// The first state is юust the augmented grammar with the expected symbols all
// at the first symbol of each rule, i.e. exactly what happens when an Item is
// instantiated from a grammar rule.
void LR0::initializeFirstState (States& states) const
{
  LR0::Closure result;
  for (unsigned int r = 0; r < _augmented.size (); ++r)
  {
    Item item (_augmented[r]);
    item.setGrammarRuleIndex (r);
    result.push_back (item);
  }

  states.push_back (result);
}

////////////////////////////////////////////////////////////////////////////////
void LR0::closeState (States& states, const int state) const
{
  // Obtain all the expected symbols for this state.
  auto expectedSymbols = getExpectedSymbols (states[state]);
  for (auto& expected : expectedSymbols)
  {
    // This will be the new state.
    Closure closure;

    // Track additional symbols.
    std::set <std::string> seen;

    // Find all the rules in this state that are expecting 'expected'.
    for (auto& item : states[state])
    {
      if (! item.done () &&
          item.next () == expected)
      {
        Item advanced (item);
        advanced.advance ();
        closure.push_back (advanced);

        if (! advanced.done ())
        {
          auto nextSymbol = advanced.next ();
          if (seen.find (nextSymbol) == seen.end ())
          {
            for (unsigned int r = 0; r < _augmented.size (); ++r)
            {
              if (_augmented[r][0] == nextSymbol)
              {
                Item additional (_augmented[r]);
                additional.setGrammarRuleIndex (r);
                closure.push_back (additional);
                seen.insert (nextSymbol);
              }
            }
          }
        }
      }
    }

    // Check that the new state is not already created.
    bool skip = false;
    for (auto& state : states)
      if (state[0] == closure[0])
        skip = true;

    // Create the new state, and recurse to close it.
    if (! skip)
    {
      states.push_back (closure);
      closeState (states, states.size () - 1);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
void LR0::createParseTable (States& states)
{
  // Add artificial end-point.
  _terminals.push_back ("$");

  // First size the tables:
  // - Create a row for every state
  // - Create a column for every terminal in the _actions table
  // - Create a column for every rule in the _goto table
  for (unsigned int state = 0; state < states.size (); ++state)
  {
    // Create a column for every terminal.
    _actions.push_back ({});
    for (auto& terminal : _terminals)
      _actions[state][terminal] = "";

    _goto.push_back ({});
    for (auto& rule : _rules)
      _goto[state][rule] = "";
  }

  // Look for states with items that are done, and mark them as reductions, but
  // a reduction of grammar rule 0 is marked "acc".
  for (unsigned int state = 0; state < states.size (); ++state)
    for (unsigned int item = 0; item < states[state].size (); ++item)
      if (states[state][item].done ())
      {
        if (states[state][item]._grammarRule == 0)
          _actions[state]["$"] = "acc";
        else
          for (auto& terminal : _terminals)
            _actions[state][terminal] = format ("r{1}", states[state][item]._grammarRule);
      }

  // Look at all items in all states, and if the item is not done, find the
  // state with a matching, advanced, first item.
  for (unsigned int state = 0; state < states.size (); ++state)
    for (unsigned int item = 0; item < states[state].size (); ++item)
      if (! states[state][item].done ())
      {
        auto nextSymbol = states[state][item].next ();

        Item advanced (states[state][item]);
        advanced.advance ();

        for (unsigned int s = 0; s < states.size (); ++s)
        {
          if (states[s][0] == advanced)
          {
            if (std::find (_terminals.begin (), _terminals.end (), nextSymbol) != _terminals.end ())
              _actions[state][nextSymbol] = format ("s{1}", s);
            else
              _goto[state][nextSymbol] = format ("s{1}", s);
          }
        }
      }
}

////////////////////////////////////////////////////////////////////////////////
void LR0::parse (const std::string& input)
{
  Lexer l (input);

  std::string token;
  Lexer::Type type;
  while (l.token (token, type))
  {
    std::cout << "# token '" << token << "' " << Lexer::typeToString (type) << "\n";

    // TODO Done

    // TODO Shift

    // TODO Reduce

    // TODO Error

  }
}

////////////////////////////////////////////////////////////////////////////////
void LR0::debug (bool value)
{
  _debug = value;
}

////////////////////////////////////////////////////////////////////////////////
//  +-------+---------------+---------------+
//  | state | terminals   $ | non-terminals |
//  +-------+--+--+--+--+---+----+-----+----+
//  |       |  |  |  |  |   |    |     |    |
//  +-------+--+--+--+--+---+----+-----+----+
//  |       |  |  |  |  |   |    |     |    |
//  +-------+--+--+--+--+---+----+-----+----+
std::string LR0::dump () const
{
  std::stringstream out;
  out << "Parser Tables\n";

  Table t;
  t.leftMargin (2);
  t.colorHeader (Color ("underline"));

  // Add columns.
  t.add ("State", true);
  for (auto& terminal : _terminals)
    t.add (terminal, true);

  for (auto& rule : _rules)
    t.add (rule, true);

  // Add cells.
  for (unsigned int state = 0; state < _actions.size (); ++state)
  {
    int row = t.addRow ();
    int col = 0;
    t.set (row, col++, state);

    for (auto& terminal : _terminals)
    {
      auto data = _actions[state].at (terminal);
      Color color (data[0] == 'r' ? "rgb535 on rgb412" :
                   data[0] == 's' ? "rgb045 on rgb015" :
                   data[0] == 'a' ? "rgb154 on rgb031" :
                   "");
      t.set (row, col++, data, color);
    }

    for (auto& rule : _rules)
    {
      auto data = _goto[state].at (rule);
      Color color (data != "" ? "rgb045 on rgb015" : "");
      t.set (row, col++, data, color);
    }
  }

  out << t.render ();

  return out.str ();
}

////////////////////////////////////////////////////////////////////////////////
LR0::Item::Item (const std::vector <std::string>& rule)
: _rule (rule)
, _cursor (2)
, _grammarRule (-1)
{
  if (_rule.size () == 3 && _rule[2] == "є")
    _rule.pop_back ();
}

////////////////////////////////////////////////////////////////////////////////
bool LR0::Item::operator== (const Item& other)
{
  if (_cursor        != other._cursor        ||
      _grammarRule   != other._grammarRule   ||
      _rule.size ()  != other._rule.size ())
    return false;

  for (unsigned int i = 0; i < _rule.size () - 1; ++i)
    if (_rule[i] != other._rule[i])
      return false;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
void LR0::Item::setGrammarRuleIndex (const int rule)
{
  _grammarRule = rule;
}

////////////////////////////////////////////////////////////////////////////////
bool LR0::Item::advance ()
{
  if (_cursor >= _rule.size ())
    return false;

  ++_cursor;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
bool LR0::Item::done () const
{
  return _cursor >= _rule.size ();
}

////////////////////////////////////////////////////////////////////////////////
std::string LR0::Item::next () const
{
  return _rule[_cursor];
}

////////////////////////////////////////////////////////////////////////////////
std::string LR0::Item::dump () const
{
  std::stringstream out;

  for (unsigned int i = 0; i < _rule.size (); ++i)
  {
    if (i)
      out << " ";

    if (i == _cursor)
      out << "● ";

    out << _rule[i];
  }

  if (_cursor >= _rule.size ())
    out << " ●";

  if (_grammarRule != -1)
    out << " [g" << _grammarRule << "]";

  return out.str ();
}

////////////////////////////////////////////////////////////////////////////////
std::string LR0::States::dump () const
{
  std::stringstream out;
  out << "States\n";

  for (unsigned int state = 0; state < this->size (); ++state)
  {
    out << "  State " << state << "\n";
    for (auto& item : (*this)[state])
      out << "    " << item.dump () << "\n";
  }

  return out.str ();
}

////////////////////////////////////////////////////////////////////////////////
