"""Synthesize and import the short original game sounds used by the loadout."""
import math
from pathlib import Path
import random
import struct
import wave
import unreal as ue

folder = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_saved_dir())) / "ClassicAudio"
folder.mkdir(exist_ok=True)
# Each layer supplies its onset, decay, start/end pitch and noise balance.
sounds = {
    "Rifle": [(.0,.051,185,52,.77),(.014,.084,95,42,.40),(.06,.038,760,260,.64)],
    "Pistol": [(.0,.037,295,86,.42),(.019,.027,160,75,.36),(.052,.014,780,480,.50)],
    "Swing": [(.0,.084,540,110,.97),(.045,.045,1200,500,.88)],
    "Draw": [(.0,.04,440,85,.68),(.083,.024,1100,460,.56)],
    "Reload": [(.04,.027,530,270,.7),(.26,.04,320,89,.71),(.64,.027,690,180,.61),(.74,.035,440,105,.5)],
    "Empty": [(.0,.009,1600,530,.55),(.018,.01,470,180,.4)],
    "Hit": [(.0,.021,1360,1030,.12),(.023,.033,1850,1450,.09)]
}
for name,layers in sounds.items():
    if ue.EditorAssetLibrary.does_asset_exist("/Game/ClassicFPS/Audio/S_"+name):
        continue
    rate=44100
    samples=[0.] * int((max(start+decay*7 for start,decay,*_ in layers)+.01)*rate)
    noise=random.Random(name)
    for start,decay,f0,f1,mix in layers:
        phase=0.
        colored=0.
        for i in range(int(decay*7*rate)):
            t=i/rate
            envelope=math.exp(-t/decay)*min(1.,t/.001)
            phase+=math.tau*(f1+(f0-f1)*math.exp(-t/(decay*.75)))/rate
            colored=.63*colored+.37*noise.uniform(-1,1)
            samples[int(start*rate)+i]+=envelope*((1-mix)*math.sin(phase)+mix*colored*2)
    peak=max(abs(v) for v in samples)
    path=folder/("S_"+name+".wav")
    with wave.open(str(path),"wb") as wav:
        wav.setparams((1,2,rate,0,"NONE",""))
        wav.writeframes(b"".join(struct.pack("<h",int(v/peak*24400)) for v in samples))
    task=ue.AssetImportTask()
    task.filename=str(path)
    task.destination_path="/Game/ClassicFPS/Audio"
    task.automated=True
    task.save=True
    ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assert task.imported_object_paths, name
ue.log("CLASSIC_AUDIO_READY")
