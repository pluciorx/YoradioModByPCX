#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include <Arduino.h>

// Animation frame structure for 20x2 LCD
struct AnimFrame {
	const char* line1;
	const char* line2;
};

// Define animation sequences
const AnimFrame fishAnimation[] = {
  {">'>            ", "        >'>     "},
  {" >'>           ", "          >'>   "},
  {"  >'>          ", "            >'> "},
  {"   >'>         ", "           >'>  "},
  {"    >'>        ", "         >'>    "},
  {"     >'>       ", "       >'>      "},
  {"      >'>      ", "     >'>        "},
  {"       >'>     ", "   >'>          "},
  {"        >'>    ", " >'>            "},
  {"         >'>   ", ">'>             "},
  {"          >'>  ", "'>              "},
  {"           >'> ", ">               "},
  {"            >'>", "                "},
  {"           >'> ", "                "},
  {"          >'>  ", "                "},
  {"         >'>   ", ">               "},
};

const AnimFrame starAnimation[] = {
  {"*    .    *    .   ", ".   *    .    *    "},
  {" *    .    *    .  ", "  .   *    .    *  "},
  {"  *    .    *    . ", "   .   *    .    * "},
  {"   *    .    *    .", "    .   *    .    *"},
};

const AnimFrame waveAnimation[] = {
  {"~~~~~~~~~~~~~~~~~~~~", "                    "},
  {" ~~~~~~~~~~~~~~~~~~~~", "~                   "},
  {"  ~~~~~~~~~~~~~~~~~~~", "~~                  "},
  {"   ~~~~~~~~~~~~~~~~~~", "~~~                 "},
  {"    ~~~~~~~~~~~~~~~~~", "~~~~                "},
  {"     ~~~~~~~~~~~~~~~~", "~~~~~               "},
};

// Bouncing ball animation for 20x2
const AnimFrame ballAnimation[] = {
  {"         o          ", "                    "},
  {"        o           ", "                    "},
  {"       o            ", "                    "},
  {"      o             ", "                    "},
  {"     o              ", "                    "},
  {"    o               ", "                    "},
  {"   o                ", "                    "},
  {"  o                 ", "                    "},
  {" o                  ", "                    "},
  {"o                   ", "                    "},
  {" o                  ", "                    "},
  {"  o                 ", "                    "},
  {"   o                ", "                    "},
  {"    o               ", "                    "},
  {"     o              ", "                    "},
  {"      o             ", "                    "},
  {"       o            ", "                    "},
  {"        o           ", "                    "},
};

const AnimFrame snakeAnimation[] = {
  {"S                   ", "                    "},
  {" S                  ", "                    "},
  {"  S                 ", "                    "},
  {"   S                ", "                    "},
  {"    S               ", "                    "},
  {"     S              ", "                    "},
  {"      S             ", "                    "},
  {"       S            ", "                    "},
  {"        S           ", "                    "},
  {"         S          ", "                    "},
  // reverse (exclude endpoint duplicate)
  {"        S           ", "                    "},
  {"       S            ", "                    "},
  {"      S             ", "                    "},
  {"     S              ", "                    "},
  {"    S               ", "                    "},
  {"   S                ", "                    "},
  {"  S                 ", "                    "},
  {" S                  ", "                    "}
};

// Fish animation for 40x2
const AnimFrame fishAnimation40[] = {
  {">'>                                    ", "                                    >'>"},
  {" >'>                                   ", "                                   >'>"},
  {"  >'>                                  ", "                                  >'>"},
  {"   >'>                                 ", "                                 >'>"},
  {"    >'>                                ", "                                >'>"},
  {"     >'>                               ", "                               >'>"},
  {"      >'>                              ", "                              >'>"},
  {"       >'>                             ", "                             >'>"},
  {"        >'>                            ", "                            >'>"},
  {"         >'>                           ", "                           >'>"},
  {"          >'>                          ", "                          >'>"},
  {"           >'>                         ", "                         >'>"},
  {"            >'>                        ", "                        >'>"},
  {"             >'>                       ", "                       >'>"},
  {"              >'>                      ", "                      >'>"},
  {"               >'>                     ", "                     >'>"},
  {"                >'>                    ", "                    >'>"},
  {"                 >'>                   ", "                   >'>"},
  {"                  >'>                  ", "                  >'>"},
  {"                   >'>                 ", "                 >'>"},
  {"                    >'>                ", "                >'>"},
  {"                     >'>               ", "               >'>"},
  {"                      >'>              ", "              >'>"},
  {"                       >'>             ", "             >'>"},
  {"                        >'>            ", "            >'>"},
  {"                         >'>           ", "           >'>"},
  {"                          >'>          ", "          >'>"},
  {"                           >'>         ", "         >'>"},
  {"                            >'>        ", "        >'>"},
  {"                             >'>       ", "       >'>"},
  {"                              >'>      ", "      >'>"},
  {"                               >'>     ", "     >'>"},
  {"                                >'>    ", "    >'>"},
  {"                                 >'>   ", "   >'>"},
  {"                                  >'>  ", "  >'>"},
  {"                                   >'> ", " >'>"},
  {"                                    >'>", ">'>" }
};

// Star animation for 40x2
const AnimFrame starAnimation40[] = {
  {"*    .    *    .    *    .    *    .  ", ".    *    .    *    .    *    .    *   "},
  {" *    .    *    .    *    .    *    . ", "  .    *    .    *    .    *    .    * "},
  {"  *    .    *    .    *    .    *    .", "   .    *    .    *    .    *    .    *"},
  {"   *    .    *    .    *    .    *    ", "    .   *    .    *    .    *    .    "}
};

// Wave animation for 40x2
const AnimFrame waveAnimation40[] = {
  {"~~~~~     ~~~~~     ~~~~~     ~~~~~   ", "     ~~~~~     ~~~~~     ~~~~~        "},
  {" ~~~~~     ~~~~~     ~~~~~     ~~~~ ~", "    ~~~~~     ~~~~~     ~~~~~     ~~~~"},
  {"~ ~~~~~     ~~~~~     ~~~~~     ~~~~  ", "   ~~~~~     ~~~~~     ~~~~~     ~~~~~"},
  {"~~ ~~~~~     ~~~~~     ~~~~~     ~~~  ", "  ~~~~~     ~~~~~     ~~~~~     ~~~~~~"},
  {"~~~ ~~~~~     ~~~~~     ~~~~~     ~~  ", " ~~~~~     ~~~~~     ~~~~~     ~~~~~~~"},
  {"~~~~ ~~~~~     ~~~~~     ~~~~~     ~  ", "~~~~~     ~~~~~     ~~~~~     ~~~~~~~~"},
  {"~~~~~ ~~~~~     ~~~~~     ~~~~~       ", "~~~~     ~~~~~     ~~~~~     ~~~~~~~~~"},
  {"~~~~~~ ~~~~~     ~~~~~     ~~~~~      ", "~~~     ~~~~~     ~~~~~     ~~~~~~~~~~"},
  {"~~~~~~~ ~~~~~     ~~~~~     ~~~~~     ", "~~     ~~~~~     ~~~~~     ~~~~~~~~~~~"},
  {"~~~~~~~~ ~~~~~     ~~~~~     ~~~~~    ", "~     ~~~~~     ~~~~~     ~~~~~~~~~~~~"},
};

// Ball animation for 40x2 — top moves left->right, bottom moves right->left (opposite)
const AnimFrame ballAnimation40[] = {
  {"o                                       ", "                                       o"},
  {" o                                      ", "                                      o "},
  {"  o                                     ", "                                     o  "},
  {"   o                                    ", "                                    o   "},
  {"    o                                   ", "                                   o    "},
  {"     o                                  ", "                                  o     "},
  {"      o                                 ", "                                 o      "},
  {"       o                                ", "                                o       "},
  {"        o                               ", "                               o        "},
  {"         o                              ", "                              o         "},
  {"          o                             ", "                             o          "},
  {"           o                            ", "                            o           "},
  {"            o                           ", "                           o            "},
  {"             o                          ", "                          o             "},
  {"              o                         ", "                         o              "},
  {"               o                        ", "                        o               "},
  {"                o                       ", "                       o                "},
  {"                 o                      ", "                      o                 "},
  {"                  o                     ", "                     o                  "},
  {"                   o                    ", "                    o                   "},
  {"                    o                   ", "                   o                    "},
  {"                     o                  ", "                  o                     "},
  {"                      o                 ", "                 o                      "},
  {"                       o                ", "                o                       "},
  {"                        o               ", "               o                        "},
  {"                         o              ", "              o                         "},
  {"                          o             ", "             o                          "},
  {"                           o            ", "            o                           "},
  {"                            o           ", "           o                            "},
  {"                             o          ", "          o                             "},
  {"                              o         ", "         o                              "},
  {"                               o        ", "        o                               "},
  {"                                o       ", "       o                                "},
  {"                                 o      ", "      o                                 "},
  {"                                  o     ", "     o                                  "},
  {"                                   o    ", "    o                                   "},
  {"                                    o   ", "   o                                    "},
  {"                                     o  ", "  o                                     "},
  {"                                      o ", " o                                      "},
  {"                                       o", "o                                       "}
};

// Snake animation for 40x2 — snake eats dots across top row, tail follows
const AnimFrame snakeAnimation40[] = {
	// ===== MOVING RIGHT =====
	{"#O>         o              o            ", "                                        "},
	{" #O>        o              o            ", "                                        "},
	{"  #O>       o              o            ", "                                        "},
	{"   #O>      o              o            ", "                                        "},
	{"    #O>     o              o            ", "                                        "},
	{"     #O>    o              o            ", "                                        "},
	{"      #O>   o              o            ", "                                        "},
	{"       #O>  o              o            ", "                                        "},
	{"        #O> o              o            ", "                                        "},
	{"         #O>o              o            ", "                                        "},
	// Eat first food
	{"          ##O<o            o            ", "                                        "},
	{"           ##O>            o            ", "                                        "},	
	{"            ##O>           o            ", "                                        "},
	{"             ##O>          o            ", "                                        "},
	{"              ##O>         o            ", "                                        "},
	{"               ##O>        o            ", "                                        "},
	{"                ##O>       o            ", "                                        "},
	{"                 ##O>      o            ", "                                        "},
	{"                  ##O>     o            ", "                                        "},
	{"                   ##O>    o            ", "                                        "},
	{"                    ##O>   o            ", "                                        "},
	{"                     ##O>  o            ", "                                        "},
	{"                      ##O> o            ", "                                        "},
	{"                       ##O>o            ", "                                        "},
	{"                       ##O<o            ", "                                        "},
	{"                        ###O>           ", "                                        "},	
	{"                          ###O>         ", "                                        "},
	{"                            ###O>       ", "                                        "},
	{"                             ###O>      ", "                                        "},
	{"                              ###O>     ", "                                        "},
	{"                               ###O>    ", "                                        "},
	{"                                ###O>   ", "                                        "},
	{"                                 ###O>  ", "                                        "},
	{"                                  ###O> ", "                                        "},
	{"                                   ###O>", "                                        "},

	{"                                   <O###", "                                        "},
	{"                                  <O### ", "                                        "},
	{"                                 <O###  ", "                                        "},
	{"                                <O###   ", "                                        "},
	{"                               <O###    ", "                                        "},
	{"                              <O###     ", "                                        "},
	{"                             <O###      ", "                                        "},
	{"                            <O###       ", "                                        "},
	{"                           <O###        ", "                                        "},
	{"                          <O###         ", "                                        "},
	// Food appears
	{"             o           <O###          ", "                                        "},
	{"             o          <O###           ", "                                        "},
	{"             o         <O###            ", "                                        "},
	{"             o        <O###             ", "                                        "},
	{"             o       <O###              ", "                                        "},
	{"             o      <O###               ", "                                        "},
	{"             o     <O###                ", "                                        "},
	{"             o    <O###                 ", "                                        "},
	{"             o   <O###                  ", "                                        "},
	{"             o  <O###                   ", "                                        "},
	{"             o <O###                    ", "                                        "},
	{"             o<O###                     ", "                                        "},
	{"             o>O###                     ", "                                        "},
	{"            <O####                      ", "                                        "},
	{"           <O####                       ", "                                        "},
	{"          <O####                        ", "                                        "},
	{"         <O####                         ", "                                        "},
	// Second food
	{"   o    <O####                          ", "                                        "},
	{"   o  <O####                            ", "                                        "},
	{"   o <O####                             ", "                                        "},
	{"   o<O####                              ", "                                        "},
	{"   o>O####                              ", "                                        "},
	{"   <O#####                              ", "                                        "},
	{"  <O#####                               ", "                                        "},
	{" <O#####                                ", "                                        "},
	{"<O#####                                 ", "                                        "},
};

// Bouncing clock animation for 40x2 (positions for time display)
// Note: HH:MM and DD/MM/YYYY are placeholder strings that should be
// replaced with actual time/date values at runtime when this animation type is used
const AnimFrame clockAnimation40[] = {
  {"            HH:MM                      ", "          DD/MM/YYYY                    "},
  {"               HH:MM                   ", "             DD/MM/YYYY                 "},
  {"                  HH:MM                ", "                DD/MM/YYYY              "},
  {"                     HH:MM             ", "                   DD/MM/YYYY           "},
  {"                        HH:MM          ", "                      DD/MM/YYYY        "},
  {"                     HH:MM             ", "                   DD/MM/YYYY           "},
  {"                  HH:MM                ", "                DD/MM/YYYY              "},
  {"               HH:MM                   ", "             DD/MM/YYYY                 "}
};

// Animation configuration
enum AnimationType : uint8_t {
	ANIM_FISH,
	ANIM_STARS,
	ANIM_WAVES,
	ANIM_BALL,
	ANIM_SNAKE,
	ANIM_CLOCK_ONLY,  // Keep existing clock behavior
	ANIM_SOUND_METER, // Sound meter for LCD displays
	ANIM_TYPE_COUNT   // Number of animation types
};

struct Animation {
	const AnimFrame* frames;
	uint8_t frameCount;
	uint16_t frameDuration;  // milliseconds per frame
	AnimationType type;
	char animName[20];  // Optional: name for debugging or CLI display
};


// Animation definitions
const Animation animations[] = {
  {fishAnimation, sizeof(fishAnimation) / sizeof(AnimFrame), 150, ANIM_FISH, "FISH"},
  {starAnimation, sizeof(starAnimation) / sizeof(AnimFrame), 300, ANIM_STARS, "STARS"},
  {waveAnimation, sizeof(waveAnimation) / sizeof(AnimFrame), 200, ANIM_WAVES, "WAVES"},
  {ballAnimation, sizeof(ballAnimation) / sizeof(AnimFrame), 100, ANIM_BALL, "BALL"},
  {snakeAnimation, sizeof(snakeAnimation) / sizeof(AnimFrame), 120, ANIM_SNAKE, "SNAKE"},
  {nullptr, 0, 0, ANIM_CLOCK_ONLY, "CLOCK_ONLY"},
  {nullptr, 0, 0, ANIM_SOUND_METER, "SOUND_METER" }
};

// Animation definitions for 40x2 displays
// Animation definitions for 40x2 displays
const Animation animations40[] = {
  {fishAnimation40, sizeof(fishAnimation40) / sizeof(AnimFrame), 200, ANIM_FISH, "FISH"},
  {starAnimation40, sizeof(starAnimation40) / sizeof(AnimFrame), 300, ANIM_STARS, "STARS"},
  {waveAnimation40, sizeof(waveAnimation40) / sizeof(AnimFrame), 200, ANIM_WAVES, "WAVES"},
  {ballAnimation40, sizeof(ballAnimation40) / sizeof(AnimFrame), 200, ANIM_BALL, "BALL"},
  {snakeAnimation40, sizeof(snakeAnimation40) / sizeof(AnimFrame), 200, ANIM_SNAKE, "SNAKE"},
  {clockAnimation40, sizeof(clockAnimation40) / sizeof(AnimFrame), 1000, ANIM_CLOCK_ONLY, "CLOCK_ONLY"},
  { nullptr, 0, 0, ANIM_SOUND_METER, "SOUND_METER" }
};

#endif