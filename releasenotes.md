Sonic Vitamin

Inflation 1.0.3
- improved buffering pre allocation
- try not to do repaint of metering when not in foreground
- some code linting
- decided to fix the meter to stereo, only left channel meter is used when mono.
- known issues
    - look and feel of zero clip button needs improvement
    - the bandsplit mode behavior is not confirmed to be good

Inflation 1.0.2
- improved Look and Feel of controls
- input/output meter should show clip level
- input/output meter color and scale should scale
- added initial bandsplit mode, based on TPT IIR , 240Hz, 2400Hz
- known issues
    - look and feel of zero clip button needs improvement
    - when plugin shifts from stereo to mono, the level meter is not mono.

Inflation 1.0.1
- fixed zero cliping, clipper happens just after Input level
- fixed floating point value precision in automation
- add initial implementation of level metering
- layout improved
- layout size is forced to a fixed size for now
- known issues
    - look and feel of sliders can be improved
    - layout still need to be improved futher
    - input meter should show clipped input and scale right
    - input/output meter should scale correctly, given the upper limit of input is +12db, output is 0db 
    - when plugin shifts from stereo to mono, the level meter is not mono.

Inflation 1.0.0
- Initial implementation of Inflator
- Known issues
 - zero clip doesnt seem to make a difference
 - floating point values of parameters are not clipped to 1 decimal place in automation, only appears correct on text box 
 - look and feel of sliders can be improved
 - layout can be improved
