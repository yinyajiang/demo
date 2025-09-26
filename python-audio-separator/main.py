from audio_separator.separator import Separator
import logging
import argparse
import json
import os

def outPutJson(code, message = None, data = None):
    json_data = {
        "code": code,
        "data": data,
        "message": message
    }
    json_data = json.dumps(json_data, ensure_ascii=True)
    print(json_data, flush=True)




def main():
    # 配置日志系统
    logging.basicConfig(level=logging.DEBUG, format='%(levelname)s: %(message)s')
    logger = logging.getLogger(__name__)
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=str, required=False,default="/Volumes/extern-usb/Downloads/周杰倫 Jay Chou【青花瓷 Blue and White Porcelain】-Official Music Video.mp3")
    parser.add_argument("--outdir", type=str, required=False, default="./output")
    parser.add_argument("--sepnum", type=int, required=False, default=2)
    parser.add_argument("--list-models", type=bool, required=False, default=False)
    args = parser.parse_args()
    logger.info("current dir: " + os.getcwd())

    module_dir = f"{os.getcwd()}/audio-separator-models/"
    additional_model_parameters_dir = f"{os.getcwd()}/audio-separator-models/additional_model_parameters"
    model_filename="UVR-MDX-NET-Inst_full_292.onnx"
    model_filename_hash="b06327a00d5e5fbc7d96e1781bbdb596"
    output_format = "WAV"
    output_sample_rate = 44100
    
    separator = Separator(log_level=logging.DEBUG,
        model_file_dir=module_dir,
        output_format=output_format,
        sample_rate=output_sample_rate,
        output_dir=args.outdir,
        additional_model_parameters_dir=additional_model_parameters_dir
     )
    if args.list_models:
        model_list = separator.list_supported_model_files()
        outPutJson(0, None, model_list)

    if args.sepnum != 2:
        outPutJson(-1, "sepnum must be 2")
        return
    if not os.path.exists(args.input):
        outPutJson(-1, "input file not exists")
        return
    if not os.path.exists(args.outdir):
        os.makedirs(args.outdir)
    
    separator.load_model(model_filename=model_filename, model_filename_hash=model_filename_hash)
    output_files = separator.separate(args.input)
    print(f"Separation complete! Output file(s): {' '.join(output_files)}")


if __name__ == "__main__":
    main()