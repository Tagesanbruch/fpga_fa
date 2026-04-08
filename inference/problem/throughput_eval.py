import argparse
import base64
import json
import mimetypes
import os
import sys

from openai import OpenAI
import requests

LONG_PROMPT = """
Please analyze this image in detail.
1.  First, please perform a full OCR (Optical Character Recognition), extract all visible text
    in the image, and list it in order from top-to-bottom, left-to-right.
2.  Second, please describe the main visual elements in the image, including but not limited to
    objects, people, scenery, and atmosphere.
3.  Finally, based on the extracted text and visual elements, summarize the theme
    and possible context of this image.
"""

def parse_args():
    """Parse command-line arguments."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description="Run a non-streaming test against a llama-server.")
    parser.add_argument(
        "-i", "--image",
        help="Path to the input image file.",
        default=os.path.join(script_dir, "image.png")
    )
    parser.add_argument(
        "-o", "--output",
        help="Path to save the output metrics JSON file.",
        default=os.path.join(script_dir, "throughput_metrics.json")
    )
    parser.add_argument(
        "--server-url",
        default="http://127.0.0.1:8080",
        help="llama-server base URL without the trailing /v1."
    )
    parser.add_argument(
        "--model",
        default="local-model",
        help="Model name used in the request body."
    )
    parser.add_argument(
        "--api-mode",
        choices=("completion", "chat"),
        default="completion",
        help="Use llama.cpp /completions multimodal mode or OpenAI-compatible /v1/chat/completions."
    )
    parser.add_argument(
        "--max-tokens",
        type=int,
        default=4096,
        help="Maximum generated tokens."
    )
    return parser.parse_args()


def image_to_base64(image_path):
    """Helper function: encode an image file as a Base64 string"""
    with open(image_path, "rb") as f:
        return base64.b64encode(f.read()).decode('utf-8')


def make_data_url(image_path):
    mime_type, _ = mimetypes.guess_type(image_path)
    if mime_type is None:
        mime_type = "image/png"
    return f"data:{mime_type};base64,{image_to_base64(image_path)}"


def run_completion(server_url, model, image_path, prompt_text, max_tokens):
    payload = {
        "model": model,
        "prompt": {
            "prompt_string": f"{prompt_text.rstrip()}\n<__media__>\n",
            "multimodal_data": [image_to_base64(image_path)],
        },
        "temperature": 0.0,
        "n_predict": max_tokens,
    }
    response = requests.post(
        f"{server_url.rstrip('/')}/completions",
        json=payload,
        timeout=600,
    )
    response.raise_for_status()
    body = response.json()
    content = body["content"]
    timings = body["timings"]
    return {
        "content": content,
        "prompt_tokens": body.get("tokens_evaluated", 0),
        "completion_tokens": body.get("tokens_predicted", 0),
        "total_tokens": body.get("tokens_evaluated", 0) + body.get("tokens_predicted", 0),
        "prompt_ms": timings["prompt_ms"],
        "predicted_ms": timings["predicted_ms"],
        "prompt_per_second": timings["prompt_per_second"],
        "predicted_per_second": timings["predicted_per_second"],
    }


def run_chat(server_url, model, image_path, prompt_text, max_tokens):
    client = OpenAI(
        base_url=f"{server_url.rstrip('/')}/v1",
        api_key="NA"
    )
    data_url = make_data_url(image_path)
    response = client.chat.completions.create(
        model=model,
        messages=[
            {
                "role": "user",
                "content": [
                    {"type": "image_url", "image_url": {"url": data_url}},
                    {"type": "text", "text": prompt_text},
                ],
            }
        ],
        max_tokens=max_tokens,
        temperature=0.0,
        stream=False,
    )
    timings = response.timings
    usage = response.usage
    return {
        "content": response.choices[0].message.content,
        "prompt_tokens": usage.prompt_tokens,
        "completion_tokens": usage.completion_tokens,
        "total_tokens": usage.total_tokens,
        "prompt_ms": timings["prompt_ms"],
        "predicted_ms": timings["predicted_ms"],
        "prompt_per_second": timings["prompt_per_second"],
        "predicted_per_second": timings["predicted_per_second"],
    }

def main():
    args = parse_args()

    # Check if image exists
    if not os.path.exists(args.image):
        print(f"Error: Image path not found: {args.image}")
        sys.exit(1)

    try:
        if args.api_mode == "completion":
            result = run_completion(args.server_url, args.model, args.image, LONG_PROMPT, args.max_tokens)
        else:
            result = run_chat(args.server_url, args.model, args.image, LONG_PROMPT, args.max_tokens)

        full_response = result["content"]
        print(full_response)
        print("\n" + "--- Generation Finished ---")
        
        # Parse and print metrics from the response object
        print("\n--- Performance Metrics (from llama-server) ---")

        print(f"[Token Stats]")
        print(f"  Prompt Tokens:     {result['prompt_tokens']} tokens")
        print(f"  Completion Tokens: {result['completion_tokens']} tokens")
        print(f"  Total Tokens:      {result['total_tokens']} tokens")
        
        print(f"\n[Server-Side Timing (ms)]")
        print(f"  Prefill Time: {result['prompt_ms']:.2f} ms")
        print(f"  Decode Time:  {result['predicted_ms']:.2f} ms")
        print(f"  Total Time (Server): {(result['prompt_ms'] + result['predicted_ms']):.2f} ms")

        prefill_speed = result['prompt_per_second']
        decode_speed = result['predicted_per_second']

        print(f"\n[Speed (Tokens/sec)]")
        print(f"  Prefill Speed:  {prefill_speed:.2f} t/s")
        print(f"  Decode Speed:   {decode_speed:.2f} t/s")

        # --- Save metrics to JSON ---
        metrics_data = {
            "api_mode": args.api_mode,
            "model": args.model,
            "prefill_speed_tps": prefill_speed,
            "decode_speed_tps": decode_speed,
            "prompt_tokens": result["prompt_tokens"],
            "completion_tokens": result["completion_tokens"],
        }
        
        # Use the output path from command-line arguments
        output_json_path = args.output

        try:
            with open(output_json_path, 'w', encoding='utf-8') as f:
                json.dump(metrics_data, f, indent=4)
            print(f"\nSuccessfully saved metrics to: {output_json_path}")
        except IOError as e:
            print(f"\nError: Failed to write metrics file: {e}")
        # --- End of JSON saving ---

    except Exception as e:
        print(f"\n\n--- AN ERROR OCCURRED ---")
        print(f"Error Type: {type(e).__name__}")
        print(f"Error Message: {e}")

if __name__ == "__main__":
    main()
