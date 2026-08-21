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
Source: "allvst3\ConsoleDrive.vst3\*"; DestDir: "{app}\ConsoleDrive.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\DistortionGuitar.vst3\*"; DestDir: "{app}\DistortionGuitar.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\FuzzGuitar.vst3\*"; DestDir: "{app}\FuzzGuitar.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\Overdrive.vst3\*"; DestDir: "{app}\Overdrive.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\Preamp.vst3\*"; DestDir: "{app}\Preamp.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\CleanUpPro.vst3\*"; DestDir: "{app}\CleanUpPro.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\ToneSculptor.vst3\*"; DestDir: "{app}\ToneSculptor.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\AmpSim.vst3\*"; DestDir: "{app}\AmpSim.vst3"; Flags: recursesubdirs; Components: drive
Source: "allvst3\Compressor.vst3\*"; DestDir: "{app}\Compressor.vst3"; Flags: recursesubdirs; Components: dynamics
Source: "allvst3\BroadcastCompressor.vst3\*"; DestDir: "{app}\BroadcastCompressor.vst3"; Flags: recursesubdirs; Components: dynamics
Source: "allvst3\MultibandCompressor.vst3\*"; DestDir: "{app}\MultibandCompressor.vst3"; Flags: recursesubdirs; Components: dynamics
Source: "allvst3\NoiseGate.vst3\*"; DestDir: "{app}\NoiseGate.vst3"; Flags: recursesubdirs; Components: dynamics
Source: "allvst3\TransientShaper.vst3\*"; DestDir: "{app}\TransientShaper.vst3"; Flags: recursesubdirs; Components: dynamics
Source: "allvst3\Deesser.vst3\*"; DestDir: "{app}\Deesser.vst3"; Flags: recursesubdirs; Components: dynamics
Source: "allvst3\AutoSwell.vst3\*"; DestDir: "{app}\AutoSwell.vst3"; Flags: recursesubdirs; Components: dynamics
Source: "allvst3\Chorus.vst3\*"; DestDir: "{app}\Chorus.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Flanger.vst3\*"; DestDir: "{app}\Flanger.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Phaser.vst3\*"; DestDir: "{app}\Phaser.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Tremolo.vst3\*"; DestDir: "{app}\Tremolo.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Vibrato.vst3\*"; DestDir: "{app}\Vibrato.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\RotarySpeaker.vst3\*"; DestDir: "{app}\RotarySpeaker.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Doubler.vst3\*"; DestDir: "{app}\Doubler.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\RingModulator.vst3\*"; DestDir: "{app}\RingModulator.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\EnvelopeFilterGuitar.vst3\*"; DestDir: "{app}\EnvelopeFilterGuitar.vst3"; Flags: recursesubdirs; Components: modulation
Source: "allvst3\Delay.vst3\*"; DestDir: "{app}\Delay.vst3"; Flags: recursesubdirs; Components: delayreverb
Source: "allvst3\TapeDelay.vst3\*"; DestDir: "{app}\TapeDelay.vst3"; Flags: recursesubdirs; Components: delayreverb
Source: "allvst3\Reverb.vst3\*"; DestDir: "{app}\Reverb.vst3"; Flags: recursesubdirs; Components: delayreverb
Source: "allvst3\SpringReverb.vst3\*"; DestDir: "{app}\SpringReverb.vst3"; Flags: recursesubdirs; Components: delayreverb
Source: "allvst3\ConvolutionReverb.vst3\*"; DestDir: "{app}\ConvolutionReverb.vst3"; Flags: recursesubdirs; Components: delayreverb
Source: "allvst3\EQ.vst3\*"; DestDir: "{app}\EQ.vst3"; Flags: recursesubdirs; Components: eq
Source: "allvst3\GraphicEQ.vst3\*"; DestDir: "{app}\GraphicEQ.vst3"; Flags: recursesubdirs; Components: eq
Source: "allvst3\AirEnhancer.vst3\*"; DestDir: "{app}\AirEnhancer.vst3"; Flags: recursesubdirs; Components: eq
Source: "allvst3\Exciter.vst3\*"; DestDir: "{app}\Exciter.vst3"; Flags: recursesubdirs; Components: eq
Source: "allvst3\PhaseAlign.vst3\*"; DestDir: "{app}\PhaseAlign.vst3"; Flags: recursesubdirs; Components: eq
Source: "allvst3\ConsoleEQ.vst3\*"; DestDir: "{app}\ConsoleEQ.vst3"; Flags: recursesubdirs; Components: eq
Source: "allvst3\Bitcrusher.vst3\*"; DestDir: "{app}\Bitcrusher.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\GlitchMachine.vst3\*"; DestDir: "{app}\GlitchMachine.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\Granulator.vst3\*"; DestDir: "{app}\Granulator.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\VinylEmulation.vst3\*"; DestDir: "{app}\VinylEmulation.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\TapeEmulation.vst3\*"; DestDir: "{app}\TapeEmulation.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\OctaverGuitar.vst3\*"; DestDir: "{app}\OctaverGuitar.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\SynthGuitar.vst3\*"; DestDir: "{app}\SynthGuitar.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\SynthBass.vst3\*"; DestDir: "{app}\SynthBass.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\VocalShifter.vst3\*"; DestDir: "{app}\VocalShifter.vst3"; Flags: recursesubdirs; Components: texturesynth
Source: "allvst3\UtilityGain.vst3\*"; DestDir: "{app}\UtilityGain.vst3"; Flags: recursesubdirs; Components: utility
Source: "allvst3\MonoMaker.vst3\*"; DestDir: "{app}\MonoMaker.vst3"; Flags: recursesubdirs; Components: utility
Source: "allvst3\DrumEnhancer.vst3\*"; DestDir: "{app}\DrumEnhancer.vst3"; Flags: recursesubdirs; Components: utility

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssDone then
    MsgBox('Thanks for installing DevKomodo Pedals.' + #13#10 +
      'Support and license: {#MyAppURL}', mbInformation, MB_OK);
end;