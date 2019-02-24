# arnie

Arnie is a quick and dirty IRC bot that uses a simple text-based events scripting language to handle IRC events.
This is an unfinished project but might work depending on the day and time.  It needs to be cleaned up a lot.

## Sample Events File
```
PRIVMSG:arnie:#:privmsg $target what?
PRIVMSG:arnie opme:#mirc:privmsg $target No
MODE:+o arnie:#:privmsg $target Thanks $sender!
MODE:+o *:#:privmsg $target congrats.
```

## Sample Startup File
```
JOIN #mirc
```

## TODO

1. WILDCARD MATCH IDENTIFIERS
2. MULTI COMMAND EVENTS
3. SHELL COMMAND HOOKS

## Copyright

Copyright (c) 2019 Andrew Lee.
