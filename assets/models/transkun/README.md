# Transkun (piano transcription model)

Used in the background to learn your keyboard's sound from your playing (Settings →
Your keyboard's sound). Not needed for practice itself.

- `transkun.onnx`: Transkun 2.0 by Yujia Yan (MIT licence, see `LICENSE.transkun`),
  <https://github.com/Yujia-Yan/Skipping-The-Frame-Level>, exported to ONNX by
  audio-claudio (TuesdayCrowd, public domain): <https://huggingface.co/TuesdayCrowd/transkun-onnx>.
- `freq2mels.f32`, `windows.f32`: the model's sound-analysis buffers (from the same export).
