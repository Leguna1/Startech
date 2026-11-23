"""Builds Startech's audio setup (TASK_Audio_1): sound groups, mixes, attenuation, concurrency, our copies of the
effect sounds, the music playlists, and the ammo assets' sounds.

Run headless with the editor closed (after building, so MusicPlaylist exists):
  UnrealEditor-Cmd.exe "<Project>.uproject" -run=pythonscript
      -script="<Project>/Tools/Audio/setup_audio.py [classes] [mixes] [attenuation] [concurrency] [sfx] [music] [ammo]"
      -unattended -nopause -nosplash -stdout
No step names runs every step. Idempotent: every asset is rewritten from the tables below, so change them here.

Does:
  classes      /Game/Audio/Classes  SC_Master > SC_Music, SC_MenuMusic, SC_SFX > SC_Weapons, SC_Explosions
  mixes        /Game/Audio/Mixes    SMix_Volumes (one adjuster per class at 1.0; Master applies to its children; the
                                    game sets the volumes at runtime) and SMix_Hub (SFX and its children to 0, 0.5 s
                                    fades; pushed while the station hub is open)
  attenuation  /Game/Audio/Attenuation  ATT_Combat (full to 30 m, silent at 300 m), ATT_Explosion (50 m / 500 m)
  concurrency  /Game/Audio/Concurrency  one per sound: CON_LaserShot 8, CON_LaserHit 8, CON_Explosion 4,
                                        CON_Beam 4, CON_Charge 4, CON_Warp 1; the oldest stops first
  sfx          /Game/Audio/SFX      our copies of /Game/Sound (the originals are never changed): trimmed laser start
                                    (from its first sound) and hit (around its loudest point, the impact, with a
                                    short fade-in), TRIM_SECONDS long with a FADE_SECONDS fade-out, re-made from the
                                    source on every run, and plain copies of the explosion, the charge-up,
                                    the beam loop (looping) and the warp (made once; delete one to copy it again),
                                    each with its class, attenuation and concurrency
  music        /Game/Audio/Music    one MusicPlaylist per entry in PLAYLISTS: every wave in that pack folder
  ammo         laser ammo: StartSound SW_Laser_Start, TravelSound none, HitSound SW_Laser_Hit;
               shield-drain ammo: TravelSound SW_Beam_Loop
The game reads the classes, mixes, playlists and the warp sound from UStartechSettings::Audio, the death sound from
Death.DefaultEffects and the charge-up from ChargedShot (DefaultGame.ini). Results are reported with
unreal.log_warning, because plain unreal.log does not reach stdout.
"""

import array
import os
import sys
import tempfile
import wave

import unreal

ROOT = "/Game/Audio"
CLASSES_DIR = ROOT + "/Classes"
MIXES_DIR = ROOT + "/Mixes"
ATT_DIR = ROOT + "/Attenuation"
CON_DIR = ROOT + "/Concurrency"
SFX_DIR = ROOT + "/SFX"
MUSIC_DIR = ROOT + "/Music"
SOURCE_DIR = "/Game/Sound"
PACK = "/Game/Harmony_of_the_World_-_Universe"

# name, parent
CLASSES = [
    ("SC_Master", None),
    ("SC_Music", "SC_Master"),
    ("SC_MenuMusic", "SC_Master"),
    ("SC_SFX", "SC_Master"),
    ("SC_Weapons", "SC_SFX"),
    ("SC_Explosions", "SC_SFX"),
]

# name: (inner radius m, silent at m)
ATTENUATION = {
    "ATT_Combat": (30.0, 300.0),
    "ATT_Explosion": (50.0, 500.0),
}

# name: max at once (oldest stops first)
CONCURRENCY = {
    "CON_LaserShot": 8,
    "CON_LaserHit": 8,
    "CON_Explosion": 4,
    "CON_Beam": 4,
    "CON_Charge": 4,
    "CON_Warp": 1,
}

TRIM_SECONDS = 0.25
FADE_SECONDS = 0.08       # fade-out at the end of every trimmed sound
PEAK_LEAD_SECONDS = 0.03  # "peak" trims start this long before the loudest sample
PEAK_FADE_IN_SECONDS = 0.01
SILENCE_THRESHOLD = 0.02  # of full scale: "start" trims cut leading samples below it

# name: (source, trim, class, attenuation, concurrency, looping)
# trim: None (a plain copy), "start" (from the first sound above SILENCE_THRESHOLD) or "peak" (around the loudest
# sample, the impact, with a short fade-in); both keep TRIM_SECONDS and fade out over FADE_SECONDS.
SFX = {
    "SW_Laser_Start": ("laser-fire", "start", "SC_Weapons", "ATT_Combat", "CON_LaserShot", False),
    "SW_Laser_Hit": ("direct-hit-sound-anime", "peak", "SC_Weapons", "ATT_Combat", "CON_LaserHit", False),
    "SW_Explosion": ("explosion-sound-effect-meme", None, "SC_Explosions", "ATT_Explosion", "CON_Explosion", False),
    "SW_Beam_Loop": ("laser-loop", None, "SC_Weapons", "ATT_Combat", "CON_Beam", True),
    "SW_ChargeUp": ("chargeup", None, "SC_Weapons", "ATT_Combat", "CON_Charge", False),
    "SW_Warp": ("laser-sfx-transition", None, "SC_SFX", None, "CON_Warp", False),
}

# playlist asset: pack folder whose /Waves holds its takes
PLAYLISTS = {
    "DA_Music_Universe": PACK,
    "DA_Music_Earth": PACK + "/Solar_System/Earth",
    "DA_Music_Mars": PACK + "/Solar_System/Mars",
    "DA_Music_Neptune": PACK + "/Solar_System/Neptune",
    "DA_Music_Jupiter": PACK + "/Solar_System/Jupiter",
}

LASER_AMMO = ["DA_1xAmmo", "DA_2x_Ammo", "DA_x3_Ammo", "DA_x4_Ammo"]
DRAIN_AMMO = ["DA_1xDrainShield", "DA_2xDrainShield"]
AMMO_DIR = "/Game/Data/AmmoAsset"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def report(message):
    unreal.log_warning("[Audio] " + message)


def load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError("Asset not found: " + path)
    return asset


def get_or_create(folder, name, asset_class, factory):
    path = folder + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return load(path)
    asset = asset_tools.create_asset(name, folder, asset_class, factory)
    if asset is None:
        raise RuntimeError("Could not create " + path)
    report("created " + path)
    return asset


def save(asset):
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)


# --- classes ---------------------------------------------------------------------------------------------------------

def setup_classes():
    classes = {}
    for name, _ in CLASSES:
        classes[name] = get_or_create(CLASSES_DIR, name, unreal.SoundClass, unreal.SoundClassFactory())
    for name, parent in CLASSES:
        sound_class = classes[name]
        sound_class.set_editor_property("parent_class", classes[parent] if parent else None)
        children = [classes[child] for child, child_parent in CLASSES if child_parent == name]
        sound_class.set_editor_property("child_classes", children)
    for sound_class in classes.values():
        save(sound_class)
    report("classes: " + ", ".join("%s < %s" % (n, p) if p else n for n, p in CLASSES))
    return classes


def get_classes():
    return {name: load(CLASSES_DIR + "/" + name) for name, _ in CLASSES}


# --- mixes -----------------------------------------------------------------------------------------------------------

def adjuster(sound_class, volume, children):
    entry = unreal.SoundClassAdjuster()
    entry.set_editor_property("sound_class_object", sound_class)
    entry.set_editor_property("volume_adjuster", volume)
    entry.set_editor_property("pitch_adjuster", 1.0)
    entry.set_editor_property("apply_to_children", children)
    return entry


def setup_mixes():
    classes = get_classes()
    volumes = get_or_create(MIXES_DIR, "SMix_Volumes", unreal.SoundMix, unreal.SoundMixFactory())
    volumes.set_editor_property("sound_class_effects", [
        adjuster(classes[name], 1.0, name == "SC_Master") for name, _ in CLASSES])
    volumes.set_editor_property("fade_in_time", 0.0)
    volumes.set_editor_property("fade_out_time", 0.0)
    volumes.set_editor_property("duration", -1.0)
    save(volumes)

    hub = get_or_create(MIXES_DIR, "SMix_Hub", unreal.SoundMix, unreal.SoundMixFactory())
    hub.set_editor_property("sound_class_effects", [adjuster(classes["SC_SFX"], 0.0, True)])
    hub.set_editor_property("fade_in_time", 0.5)
    hub.set_editor_property("fade_out_time", 0.5)
    hub.set_editor_property("duration", -1.0)
    save(hub)
    report("mixes: SMix_Volumes (%d adjusters), SMix_Hub (SFX to 0, 0.5 s fades)" % len(CLASSES))


# --- attenuation and concurrency -------------------------------------------------------------------------------------

def setup_attenuation():
    for name, (inner_m, silent_m) in ATTENUATION.items():
        asset = get_or_create(ATT_DIR, name, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
        settings = asset.get_editor_property("attenuation")
        settings.set_editor_property("attenuate", True)
        settings.set_editor_property("spatialize", True)
        settings.set_editor_property("distance_algorithm", unreal.AttenuationDistanceModel.LINEAR)
        settings.set_editor_property("attenuation_shape", unreal.AttenuationShape.SPHERE)
        settings.set_editor_property("attenuation_shape_extents", unreal.Vector(inner_m * 100.0, 0.0, 0.0))
        settings.set_editor_property("falloff_distance", (silent_m - inner_m) * 100.0)
        asset.set_editor_property("attenuation", settings)
        save(asset)
        report("attenuation: %s full to %.0f m, silent at %.0f m" % (name, inner_m, silent_m))


def setup_concurrency():
    for name, max_count in CONCURRENCY.items():
        asset = get_or_create(CON_DIR, name, unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
        settings = asset.get_editor_property("concurrency")
        settings.set_editor_property("max_count", max_count)
        settings.set_editor_property("resolution_rule", unreal.MaxConcurrentResolutionRule.STOP_OLDEST)
        settings.set_editor_property("limit_to_owner", False)
        asset.set_editor_property("concurrency", settings)
        save(asset)
        report("concurrency: %s max %d, oldest stops" % (name, max_count))


# --- sound effects ---------------------------------------------------------------------------------------------------

def export_wav(sound, path):
    task = unreal.AssetExportTask()
    task.set_editor_property("object", sound)
    task.set_editor_property("filename", path)
    task.set_editor_property("automated", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("replace_identical", True)
    if not unreal.Exporter.run_asset_export_task(task) or not os.path.exists(path):
        raise RuntimeError("Could not export %s as WAV" % sound.get_path_name())


def trim_wav(source_path, target_path, mode):
    """Keeps TRIM_SECONDS from the first sound ("start") or around the loudest sample ("peak", with a short
    fade-in), and fades the last FADE_SECONDS out. 16-bit PCM only. Returns (start s, length s, peak s or None)."""
    with wave.open(source_path, "rb") as source:
        channels = source.getnchannels()
        width = source.getsampwidth()
        rate = source.getframerate()
        frames = source.readframes(source.getnframes())
    if width != 2:
        raise RuntimeError("%s is %d-bit; only 16-bit PCM is handled" % (source_path, width * 8))
    samples = array.array("h")
    samples.frombytes(frames)
    if sys.byteorder != "little":
        samples.byteswap()

    peak_time = None
    if mode == "peak":
        loudest = max(range(len(samples)), key=lambda index: abs(samples[index]))
        peak_frame = loudest // channels
        peak_time = peak_frame / float(rate)
        first = max(0, peak_frame - int(PEAK_LEAD_SECONDS * rate)) * channels
    else:
        threshold = int(32767 * SILENCE_THRESHOLD)
        first = 0
        while first < len(samples) and abs(samples[first]) < threshold:
            first += 1
        first -= first % channels

    keep_frames = int(TRIM_SECONDS * rate)
    fade_frames = max(1, int(FADE_SECONDS * rate))
    out = samples[first:first + keep_frames * channels]
    frame_count = len(out) // channels
    if mode == "peak":
        fade_in_frames = max(1, int(PEAK_FADE_IN_SECONDS * rate))
        for frame in range(min(fade_in_frames, frame_count)):
            gain = frame / float(fade_in_frames)
            for channel in range(channels):
                index = frame * channels + channel
                out[index] = int(out[index] * gain)
    for frame in range(max(0, frame_count - fade_frames), frame_count):
        gain = (frame_count - 1 - frame) / float(fade_frames)
        for channel in range(channels):
            index = frame * channels + channel
            out[index] = int(out[index] * gain)

    if sys.byteorder != "little":
        out.byteswap()
    with wave.open(target_path, "wb") as target:
        target.setnchannels(channels)
        target.setsampwidth(width)
        target.setframerate(rate)
        target.writeframes(out.tobytes())
    return first // channels / float(rate), frame_count / float(rate), peak_time


def import_wav(file_path, name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", file_path)
    task.set_editor_property("destination_path", SFX_DIR)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    asset_tools.import_asset_tasks([task])
    return load(SFX_DIR + "/" + name)


def copy_sound(source_path, name):
    """Our copy of a source sound. An existing copy is kept (other assets reference it); delete it to copy again."""
    path = SFX_DIR + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return load(path)
    if unreal.EditorAssetLibrary.duplicate_asset(source_path, path) is None:
        raise RuntimeError("Could not copy %s to %s" % (source_path, path))
    return load(path)


def setup_sfx():
    classes = get_classes()
    work = tempfile.mkdtemp(prefix="startech_audio_")
    for name, (source, trim, class_name, att_name, con_name, looping) in SFX.items():
        source_path = SOURCE_DIR + "/" + source
        if trim:
            exported = os.path.join(work, source + ".wav")
            cut = os.path.join(work, name + ".wav")
            export_wav(load(source_path), exported)
            start, length, peak = trim_wav(exported, cut, trim)
            sound = import_wav(cut, name)
            if peak is None:
                detail = "trimmed from %s: from %.3f s (first sound), %.3f s kept, %.2f s fade-out" % (
                    source, start, length, FADE_SECONDS)
            else:
                detail = "trimmed from %s around its loudest point at %.3f s: from %.3f s, %.3f s kept, %.2f s fade-in, %.2f s fade-out" % (
                    source, peak, start, length, PEAK_FADE_IN_SECONDS, FADE_SECONDS)
        else:
            sound = copy_sound(source_path, name)
            detail = "copy of " + source
        sound.set_editor_property("sound_class_object", classes[class_name])
        sound.set_editor_property("attenuation_settings", load(ATT_DIR + "/" + att_name) if att_name else None)
        sound.set_editor_property("concurrency_set", [load(CON_DIR + "/" + con_name)] if con_name else [])
        sound.set_editor_property("looping", looping)
        save(sound)
        report("sfx: %s (%s) %s, %s, %s%s, %.2f s" % (name, detail, class_name, att_name or "2D", con_name,
                                                     ", looping" if looping else "", sound.get_editor_property("duration")))


# --- music -----------------------------------------------------------------------------------------------------------

def setup_music():
    for name, folder in PLAYLISTS.items():
        playlist = get_or_create(MUSIC_DIR, name, unreal.MusicPlaylist, unreal.DataAssetFactory())
        waves = registry.get_assets_by_path(folder + "/Waves", False)
        tracks = []
        for data in sorted(waves, key=lambda d: str(d.asset_name)):
            if str(data.asset_class_path.asset_name) != "SoundWave":
                continue
            # By object path: a few waves' inner names differ from their package names (Gaia's_Quiet_Loop).
            sound = unreal.load_object(None, "%s.%s" % (data.package_name, data.asset_name))
            if sound is None:
                raise RuntimeError("Could not load %s" % data.package_name)
            tracks.append(sound)
        if not tracks:
            raise RuntimeError("No waves in " + folder + "/Waves")
        playlist.set_editor_property("tracks", tracks)
        save(playlist)
        report("music: %s, %d takes from %s/Waves" % (name, len(tracks), folder.replace(PACK, "<pack>")))


# --- ammo ------------------------------------------------------------------------------------------------------------

def setup_ammo():
    start = load(SFX_DIR + "/SW_Laser_Start")
    hit = load(SFX_DIR + "/SW_Laser_Hit")
    beam = load(SFX_DIR + "/SW_Beam_Loop")
    for name in LASER_AMMO:
        ammo = load(AMMO_DIR + "/" + name)
        ammo.set_editor_property("start_sound", start)
        ammo.set_editor_property("travel_sound", None)
        ammo.set_editor_property("hit_sound", hit)
        save(ammo)
    for name in DRAIN_AMMO:
        ammo = load(AMMO_DIR + "/" + name)
        ammo.set_editor_property("travel_sound", beam)
        save(ammo)
    report("ammo: %s start SW_Laser_Start, hit SW_Laser_Hit, no travel; %s beam SW_Beam_Loop" % (
        ", ".join(LASER_AMMO), ", ".join(DRAIN_AMMO)))


STEPS = [("classes", setup_classes), ("mixes", setup_mixes), ("attenuation", setup_attenuation),
         ("concurrency", setup_concurrency), ("sfx", setup_sfx), ("music", setup_music), ("ammo", setup_ammo)]

requested = [arg.lower() for arg in sys.argv[1:]]
for step_name, step in STEPS:
    if not requested or step_name in requested:
        step()
report("done: " + (", ".join(requested) if requested else "every step"))
