from .extract_confirmation_scalars import main_for_dataset
from .extract_confirmation_scalars import normalized_bytes as _normalized_bytes


def normalized_bytes(raw_path):
    return _normalized_bytes(raw_path, "derby_fuller_1999")


def main(argv=None):
    main_for_dataset("derby_fuller_1999", argv)


if __name__ == "__main__":
    main()
