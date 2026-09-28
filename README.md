# Ant

Author: Sarah Garland (sgarlan2)

Design: Two ants at a picnic compete for cookie crumbs.

Networking: `Game.cpp` has send/recieve message functions. The server holds the game state 
(player ant positions, cookie crumb position, and points). The server sends the game state to clients. The server updates the game state based on inputs the clients send. 

Screen Shot:

![Screen Shot](screenshot.png)

How To Play:

- `WASD` to move (movement is similar to Snake, where you have to fully cycle to turn around)
- Walk into crumbs to eat


This game was built with [NEST](NEST.md).

