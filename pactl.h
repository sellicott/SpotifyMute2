/* pactl.h - header for pulseaudio control functions.
   Written in 2014 by Mehmet Kayaalp mkayaalp@cs.binghamton.edu

To the extent possible under law, the author(s) have dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with
this software. If not, see
<http://creativecommons.org/publicdomain/zero/1.0/>.*/

void init_pactl( void );
void wait_for_context( void );
void set_mute( int mute );
void update_sink( void );
void drain( void );
