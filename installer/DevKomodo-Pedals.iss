#define MyAppName "DevKomodo Pedals"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "DevKomodo"
#define MyAppURL "https://github.com/DevKomodo/DevKomodo-Plugins"

[Setup]
AppId={{B8E9C3A8-3B0C-4F3A-9AF8-123456789001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
SourceDir=..
DefaultDirName={commoncf64}\VST3
DisableProgramGroupPage=yes
OutputDir=..\installer-output
OutputBaseFilename=DevKomodo-Pedals-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
WizardStyle=modern
Uninstallable=no

[Types]
Name: "full"; Description: "Complete installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "drive"; Description: "Drive / Distortion"
Name: "dynamics"; Description: "Dynamics"
Name: "modulation"; Description: "Modulation"
Name: "delayreverb"; Description: "Delay / Reverb"
Name: "eq"; Description: "EQ / Tone shaping"
Name: "texturesynth"; Description: "Texture / Synth"
Name: "utility"; Description: "Utility"

[Files]
Source: "allvst3\Boost.vst3\*"; DestDir: "{app}\Boost.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\Clipper.vst3\*"; DestDir: "{app}\Clipper.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\Console Drive.vst3\*"; DestDir: "{app}\Console Drive.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\Distortion.vst3\*"; DestDir: "{app}\Distortion.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\Fuzz.vst3\*"; DestDir: "{app}\Fuzz.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\Overdrive.vst3\*"; DestDir: "{app}\Overdrive.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\Preamp.vst3\*"; DestDir: "{app}\Preamp.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\CleanUp Pro.vst3\*"; DestDir: "{app}\CleanUp Pro.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\Tone Sculptor.vst3\*"; DestDir: "{app}\Tone Sculptor.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\AmpSim.vst3\*"; DestDir: "{app}\AmpSim.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\Compressor.vst3\*"; DestDir: "{app}\Compressor.vst3"; Flags: recursesubdirs; Components: dynamics
Source: "allvst3\Broadcast Compressor.vst3\*"; DestDir: "{app}\Broadcast Compressor.vst3"; Flags: recursesubdirs; Components: dynamics
Source: "allvst3\Multiband Compressor.vst3\*"; DestDir: "{app}\Multiband Compressor.vst3"; Flags: recursesubdirs; Components: dynamics
Source: "allvst3\Noise Gate.vst3\*"; DestDir: "{app}\Noise Gate.vst3"; Flags: recursesubdirs; Components: dynamics
Source: "allvst3\Transient Shaper.vst3\*"; DestDir: "{app}\Transient Shaper.vst3"; Flags: recursesubdirs; Components: dynamics
Source: "allvst3\De-esser.vst3\*"; DestDir: "{app}\De-esser.vst3"; Flags: recursesubdirs; Components: dynamics
Source: "allvst3\Auto Swell.vst3\*"; DestDir: "{app}\Auto Swell.vst3"; Flags: recursesubdirs; Components: dynamics
Source: "allvst3\Chorus.vst3\*"; DestDir: "{app}\Chorus.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Flanger.vst3\*"; DestDir: "{app}\Flanger.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Phaser.vst3\*"; DestDir: "{app}\Phaser.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Tremolo.vst3\*"; DestDir: "{app}\Tremolo.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Vibrato.vst3\*"; DestDir: "{app}\Vibrato.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Rotary Speaker.vst3\*"; DestDir: "{app}\Rotary Speaker.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Doubler.vst3\*"; DestDir: "{app}\Doubler.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Ring Modulator.vst3\*"; DestDir: "{app}\Ring Modulator.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Envelope Filter.vst3\*"; DestDir: "{app}\Envelope Filter.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Delay.vst3\*"; DestDir: "{app}\Delay.vst3"; Flags: recursesubdirs; Components: delayreverb
Source: "allvst3\Tape Delay.vst3\*"; DestDir: "{app}\Tape Delay.vst3"; Flags: recursesubdirs; Components: delayreverb
Source: "allvst3\Reverb.vst3\*"; DestDir: "{app}\Reverb.vst3"; Flags: recursesubdirs; Components: delayreverb
Source: "allvst3\Spring Reverb.vst3\*"; DestDir: "{app}\Spring Reverb.vst3"; Flags: recursesubdirs; Components: delayreverb
Source: "allvst3\Convolution Reverb.vst3\*"; DestDir: "{app}\Convolution Reverb.vst3"; Flags: recursesubdirs; Components: delayreverb
Source: "allvst3\EQ 6-Band.vst3\*"; DestDir: "{app}\EQ 6-Band.vst3"; Flags: recursesubdirs; Components: eq
Source: "allvst3\Graphic EQ.vst3\*"; DestDir: "{app}\Graphic EQ.vst3"; Flags: recursesubdirs; Components: eq
Source: "allvst3\Air Enhancer.vst3\*"; DestDir: "{app}\Air Enhancer.vst3"; Flags: recursesubdirs; Components: eq
Source: "allvst3\Exciter.vst3\*"; DestDir: "{app}\Exciter.vst3"; Flags: recursesubdirs; Components: eq
Source: "allvst3\Phase Align.vst3\*"; DestDir: "{app}\Phase Align.vst3"; Flags: recursesubdirs; Components: eq
Source: "allvst3\Multiband Drive.vst3\*"; DestDir: "{app}\Multiband Drive.vst3"; Flags: recursesubdirs; Components: eq
Source: "allvst3\Bitcrusher.vst3\*"; DestDir: "{app}\Bitcrusher.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\Glitch Machine.vst3\*"; DestDir: "{app}\Glitch Machine.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\Granulator.vst3\*"; DestDir: "{app}\Granulator.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\Vinyl Emulation.vst3\*"; DestDir: "{app}\Vinyl Emulation.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\Tape Emulation.vst3\*"; DestDir: "{app}\Tape Emulation.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\Octaver.vst3\*"; DestDir: "{app}\Octaver.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\Synth Guitar.vst3\*"; DestDir: "{app}\Synth Guitar.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\Synth Bass.vst3\*"; DestDir: "{app}\Synth Bass.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\DevKomodo Vocal Shifter.vst3\*"; DestDir: "{app}\DevKomodo Vocal Shifter.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\Utility Gain.vst3\*"; DestDir: "{app}\Utility Gain.vst3"; Flags: recursesubdirs; Components: utility
Source: "allvst3\Mono Maker.vst3\*"; DestDir: "{app}\Mono Maker.vst3"; Flags: recursesubdirs; Components: utility
Source: "allvst3\Drum Enhancer.vst3\*"; DestDir: "{app}\Drum Enhancer.vst3"; Flags: recursesubdirs; Components: utility

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssDone then
    MsgBox('Thanks for installing DevKomodo Pedals.' + #13#10 +
      'Support and license: {#MyAppURL}', mbInformation, MB_OK);
end;