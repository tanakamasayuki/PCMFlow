// PCMFlow example: DecodeWavInfo
//
// Embeds a tiny 16-bit mono WAV in flash, opens it through PCMFlow's
// memory helper, and prints the detected format plus per-buffer sample
// peak to Serial. No external I/O — handy as a smoke test that the
// library builds and runs on your board.
//
// Change the call to `setOutputFormat()` to make PCMFlow do channel
// up/down-mix, resampling, bit-depth conversion, or gain adjustment.

#include <PCMFlow.h>

// 8 frames of a stylised sine at 8000 Hz, signed 16-bit mono.
static const uint8_t kSampleWav[] = {
    'R',
    'I',
    'F',
    'F',
    0x24,
    0x00,
    0x00,
    0x00,
    'W',
    'A',
    'V',
    'E',
    'f',
    'm',
    't',
    ' ',
    0x10,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00, // PCM
    0x01,
    0x00, // 1 channel
    0x40,
    0x1f,
    0x00,
    0x00, // 8000 Hz
    0x80,
    0x3e,
    0x00,
    0x00,
    0x02,
    0x00,
    0x10,
    0x00,
    'd',
    'a',
    't',
    'a',
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x68,
    0x5a,
    0xff,
    0x7f,
    0x68,
    0x5a,
    0x00,
    0x00,
    0x98,
    0xa5,
    0x01,
    0x80,
    0x98,
    0xa5,
};

PCMFlow audio;

void setup()
{
    Serial.begin(115200);
    delay(500);

    audio.setOutputFormat({8000, 1, 16});
    audio.setGain(1.0f);
    audio.setBufferFrames(256);

    if (!audio.open(kSampleWav))
    {
        Serial.print("audio.open() failed, error=");
        Serial.println((int)audio.lastError());
        return;
    }

    Serial.print("Codec: ");
    switch (audio.codec())
    {
    case PCMFlow::CodecKind::Wav:
        Serial.println("WAV");
        break;
    case PCMFlow::CodecKind::Mp3:
        Serial.println("MP3");
        break;
    case PCMFlow::CodecKind::Flac:
        Serial.println("FLAC");
        break;
    default:
        Serial.println("?");
        break;
    }
    Serial.print("Source: ");
    Serial.print(audio.sourceFormat().sampleRate);
    Serial.print(" Hz, ");
    Serial.print(audio.sourceFormat().channels);
    Serial.print(" ch, ");
    Serial.print(audio.sourceFormat().bitsPerSample);
    Serial.println("-bit");
    Serial.print("Output: ");
    Serial.print(audio.outputFormat().sampleRate);
    Serial.print(" Hz, ");
    Serial.print(audio.outputFormat().channels);
    Serial.print(" ch, ");
    Serial.print(audio.outputFormat().bitsPerSample);
    Serial.println("-bit");
}

void loop()
{
    audio.pump();
    if (audio.availableFrames() == 0)
    {
        if (audio.isEof())
        {
            Serial.println("EOF");
            while (true)
                delay(1000);
        }
        delay(1);
        return;
    }

    static constexpr size_t kChunkFrames = 64;
    static uint8_t buf[PCMFlow::maxBytesForFrames(kChunkFrames)];
    const size_t frames = audio.readFrames(buf, kChunkFrames);

    // This example fixes the output to mono 16-bit, so we interpret
    // `buf` as little-endian int16 directly.
    int16_t peak = 0;
    const size_t bytes = frames * audio.bytesPerFrame();
    for (size_t i = 0; i + 1 < bytes; i += 2)
    {
        const int16_t s = static_cast<int16_t>(buf[i] | (buf[i + 1] << 8));
        const int16_t v = s < 0 ? -s : s;
        if (v > peak)
            peak = v;
    }
    Serial.print("got ");
    Serial.print((unsigned)frames);
    Serial.print(" frames, peak=");
    Serial.println(peak);
}
