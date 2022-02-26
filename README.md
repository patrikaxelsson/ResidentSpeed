# ResidentSpeed

Tests the speed of your AmigaOS system residents and associated components
and makes it easy to find common performance problems.

## Concept

For each resident or other component to be tested, ResidentSpeed will
measure its read speed. The same measurement will then be done using the
highest priority memory in the system. If these results deviate too much,
it will be listed as a performance problem.

The idea behind this is that in a healthy system, most components should be
in the highest priority memory. If not, low performance will most likely be
the result.

The results has to be valued by how much the components are used. For
example anything related to exec.library should definately be as fast as
possible, as that is the most used component of the system. On the other end
we have expansion.library which is not used much, so it does not matter much
where it is located.

## Usage

### Arguments

```
1> ResidentSpeed ? 
SHOWALL/S,VERBOSE/S:
```

| Name      | Description                                                    |
| --------- | -------------------------------------------------------------- |
| SHOWALL   | Show all test results, not just potential performance problems |
| VERBOSE   | Show more data relevant to the speed measurement               |


### Examples

#### A1200, Blizzard1260, 3.1 ROM, AmigaOS3.2.1
```
1> ResidentSpeed
Type     Name                    Version Size    Loc  Speed   Speed %
Region   interrupt vector table            1.00k Chip   3.68M    10.8
Region   system stack                      6.00k Chip   3.65M    10.5
LibBase  expansion.library        40.2       554 Chip   3.68M    10.3
Resident 1234-BootPrefs0           0.0        42 Chip   3.86M    14.2
Resident exec.library             47.7    16.12k Chip   3.68M    10.6
LibBase  exec.library             47.7     1.50k Chip   3.68M    10.6
Resident ? LoadModule ?           45.16    2.49k Chip   3.65M    10.6
Resident utility.library          47.3     3.64k Chip   3.68M    10.6
Resident FileSystem.resource      47.4       556 Chip   3.68M    10.3
Resident battclock.resource       47.2     2.64k Chip   3.68M    10.7
Resident disk.resource            47.1       906 Chip   3.65M    10.6
Resident graphics.library         47.10   92.19k Chip   3.68M    10.6
Resident layers.library           46.2    12.01k Chip   3.65M    10.6
Resident gameport.device          47.1     2.53k Chip   3.68M    10.7
Resident timer.device             46.1     3.54k Chip   3.65M    10.6
Resident card.resource            47.4     2.97k Chip   3.65M    10.5
Resident keyboard.device          47.1     1.87k Chip   3.65M    10.4
Resident input.device             47.1     2.73k Chip   3.65M    10.6
Resident ramdrive.device          46.2     1.59k Chip   3.65M    10.5
Resident trackdisk.device         47.14    7.03k Chip   3.68M    10.6
Resident carddisk.device          47.2     2.41k Chip   3.65M    10.6
Resident scsi.device              47.4    15.38k Chip   3.65M    10.6
Resident intuition.library        47.51  110.65k Chip   3.68M    10.6
Resident syscheck                 47.1       104 Chip   3.82M    11.7
Resident bootmenu                 47.11   15.27k Chip   3.65M    10.6
Resident filesystem               47.4    29.68k Chip   3.65M    10.6
Resident mathieeesingbas.library  47.1     3.06k Chip   3.65M    10.5
Resident console.device           46.1    15.59k Chip   3.68M    10.6
Resident dos.library              47.30   35.16k Chip   3.68M    10.6
Resident system-startup           47.22      156 Chip   3.74M    11.5
```
ResidentSpeed shows a lot of potential performance issues for this system
and the reasons might not be obvious.

It begins with the Blizzard1260, which does not have real autoconfig memory.
This in turn leads to the following performance detrimental issues:
- LoadModule defaults to loading all residents updated in 3.2.1 into slow
  chip memory. This includes the performance crucial exec.library resident.
- The exec.library library base is allocated from slow chip memory.
- The system stack is allocated from slow chip memory.

This case can for example be fixed by a combination of tweaking the
LoadModule arguments and using several MuTools commands.

A system in this state feels really slow, too see some relative numbers on
the effect it has, look at the examples here:
http://aminet.net/package/util/misc/ScriptSpeed


#### A2000, G-Force030, 3.1 ROM, AmigaOS3.1

```
Type     Name                    Version Size    Loc  Speed   Speed %
Region   interrupt vector table            1.00k Chip   3.24M    14.7
Resident expansion.library        40.2       212 Kick   3.33M    15.3
LibBase  expansion.library        40.2       554 Chip   3.24M    15.4
Resident exec.library             40.10   13.57k Kick   3.30M    15.2
Resident diag init                40.0     2.58k Kick   3.30M    15.1
Resident utility.library          40.1     2.74k Kick   3.30M    15.2
Resident potgo.resource           37.4       310 Kick   3.30M    15.2
Resident cia.resource             39.1      1006 Kick   3.30M    15.2
Resident FileSystem.resource      40.1       414 Kick   3.28M    15.1
Resident battclock.resource       39.3     2.34k Kick   3.30M    15.2
Resident misc.resource            37.1       176 Kick   3.35M    13.7
Resident disk.resource            37.2       838 Kick   3.30M    15.5
Resident battmem.resource         39.2       484 Kick   3.31M    15.3
Resident graphics.library         40.24   96.50k Kick   3.30M    15.2
Resident layers.library           40.1    12.42k Kick   3.30M    15.2
Resident gameport.device          40.1     2.31k Kick   3.30M    15.1
Resident timer.device             39.4     3.48k Kick   3.30M    15.2
Resident card.resource            40.4     2.97k Kick   3.30M    15.2
Resident keyboard.device          40.1     1.14k Kick   3.30M    15.2
Resident input.device             40.1     4.15k Kick   3.30M    15.2
Resident keymap.library           40.4     3.12k Kick   3.30M    15.2
Resident ramdrive.device          39.35    1.48k Kick   3.30M    15.1
Resident trackdisk.device         40.1     7.25k Kick   3.30M    15.2
Resident carddisk.device          40.1     2.29k Kick   3.30M    15.1
Resident scsi.device              40.5    10.20k Kick   3.30M    15.2
Resident intuition.library        40.85  100.10k Kick   3.30M    15.2
Resident console.device           40.2    15.12k Kick   3.30M    15.2
Resident mathieeesingbas.library  40.4     4.20k Kick   3.30M    15.2
Resident syscheck                 40.0     4.71k Kick   3.30M    15.2
Resident romboot                  40.0       100 Kick   3.24M    14.9
Resident bootmenu                 40.5     3.57k Kick   3.30M    15.2
Resident alert.hook               40.0       327 Kick   3.30M    15.2
Resident strap                    40.1     3.74k Kick   3.30M    15.2
Resident filesystem               40.1       102 Kick   3.24M    14.9
Resident ramlib                   40.2     1.03k Kick   3.30M    15.2
Resident audio.device             37.10    3.88k Kick   3.30M    15.2
Resident dos.library              40.3    33.14k Kick   3.30M    15.2
Resident workbench.task           39.1       194 Kick   3.33M    15.3
Resident mathffp.library          40.1     1.15k Kick   3.30M    15.2
Resident icon.library             40.1     5.91k Kick   3.30M    15.2
Resident gadtools.library         40.4    22.89k Kick   3.30M    15.2
Resident workbench.library        40.5    68.98k Kick   3.30M    15.2
Resident con-handler              40.2     9.74k Kick   3.30M    15.2
Resident shell                    40.2    16.54k Kick   3.30M    15.2
Resident ram-handler              39.4     8.83k Kick   3.30M    15.2
```

ResidentSpeed shows a lot of potential performance issues for this system
too.

In this case the reason is not that complicated, it is simply becase
Kickstart ROM is very slow on this system compared to fast RAM.

This case can be fixed by remapping ROM to fast RAM Using the tools
shipped with the accelerator.

