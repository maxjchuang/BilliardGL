from .extract_confirmation_scalars import main_for_dataset
from .extract_confirmation_scalars import normalized_bytes as _normalized_bytes


def normalized_bytes(raw_path):
    return _normalized_bytes(raw_path, "sudo_2002")


def main(argv=None):
    main_for_dataset("sudo_2002", argv)


if __name__ == "__main__":
    main()
