from audio_separator.separator import Separator
import logging

# Initialize the Separator class (with optional configuration properties, below)
separator = Separator(log_level=logging.DEBUG)

# Load a machine learning model (if unspecified, defaults to 'model_mel_band_roformer_ep_3005_sdr_11.4360.ckpt')
separator.load_model(model_filename="UVR-MDX-NET-Inst_full_292.onnx")

# Perform the separation on specific audio files without reloading the model
output_files = separator.separate('/Volumes/extern-usb/Downloads/周杰倫 Jay Chou【青花瓷 Blue and White Porcelain】-Official Music Video.mp3')

print(f"Separation complete! Output file(s): {' '.join(output_files)}")