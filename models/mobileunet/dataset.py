import random
from pathlib import Path
from PIL import Image
import torch
from torch.utils.data import Dataset, DataLoader
import torchvision.transforms.functional as TF

DEFAULT_ROOT = "/home/alexis/Desktop/synthetic-dataset/triplet_dataset"


class TripletDataset(Dataset):
    def __init__(
        self,
        root: str | Path = DEFAULT_ROOT,
        split: str = "train",
        validation_ratio: float = 0.15,
        seed: int = 42,
    ):
        self.root = Path(root)
        self.split = split
        self.augment = (split == "train")

        orig_dir = self.root / "original_cropped"
        warp_dir = self.root / "warped_cropped"
        targ_dir = self.root / "liveportrait_cropped"

        common = sorted(
            list(
                {f.name for f in orig_dir.glob("*.png")}
                & {f.name for f in warp_dir.glob("*.png")}
                & {f.name for f in targ_dir.glob("*.png")}
            )
        )

        if not common:
            raise RuntimeError(
                f"No matching triplet PNG files found in {self.root}.\n"
                f"Files found: original={len(list(orig_dir.glob('*.png')))}, "
                f"warped={len(list(warp_dir.glob('*.png')))}, "
                f"liveportrait={len(list(targ_dir.glob('*.png')))}"
            )

        # Train/val split
        rng = random.Random(seed)
        rng.shuffle(common)

        n_val = max(1, int(len(common) * validation_ratio))
        self.files = common[n_val:] if split == "train" else common[:n_val]
        self.dirs = (orig_dir, warp_dir, targ_dir)

    def __len__(self) -> int:
        return len(self.files)

    def __getitem__(self, idx: int) -> dict[str, torch.Tensor]:
        name = self.files[idx]
        tensors = []
        for d in self.dirs:
            file_path = d / name
            with Image.open(file_path) as img:
                tensors.append(TF.to_tensor(img.convert("RGB")))

        orig, warp, target = tensors

        # Horizontal flip for training data augmentation:
        if self.augment and torch.rand(1).item() < 0.5:
            orig = TF.hflip(orig)
            warp = TF.hflip(warp)
            target = TF.hflip(target)

        return {
            "input": torch.cat([orig, warp], dim=0),  # [6, 256, 256]: 0:3=orig, 3:6=warp
            "warp": warp,                             # [3, 256, 256]
            "target": target,                         # [3, 256, 256]
        }


def get_dataloaders(
    root: str | Path = DEFAULT_ROOT,
    batch_size: int = 16,
    num_workers: int = 4,
    validation_ratio: float = 0.15,
    seed: int = 42,
) -> tuple[DataLoader, DataLoader]:
    train_dataset = TripletDataset(root, split="train", validation_ratio=validation_ratio, seed=seed)
    val_dataset = TripletDataset(root, split="val", validation_ratio=validation_ratio, seed=seed)

    use_persistent = num_workers > 0
    train_loader = DataLoader(
        train_dataset,
        batch_size=batch_size,
        shuffle=True,
        num_workers=num_workers,
        pin_memory=True,
        drop_last=True,
        persistent_workers=use_persistent,
    )
    val_loader = DataLoader(
        val_dataset,
        batch_size=batch_size,
        shuffle=False,
        num_workers=num_workers,
        pin_memory=True,
        drop_last=False,
        persistent_workers=use_persistent,
    )
    return train_loader, val_loader