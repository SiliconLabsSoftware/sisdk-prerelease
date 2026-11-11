from typing import ClassVar
from dataclasses import dataclass
from pycalcmodel.core.rm_studio_factory import RMStudioFactory
from host_py_rm_studio_internal.factory import (
    RM_Factory,
    RM_ALL_PART_FAMILY_NAMES,
    RM_S1_PART_FAMILY_NAMES,
    RM_S2_PART_FAMILY_NAMES
)

@dataclass
class RMStudioFactoryBuilder:
    factory: ClassVar[RMStudioFactory | None] = None
    first_run: ClassVar[bool] = True

    @classmethod
    def build(cls):
        if cls.first_run:
            cls.first_run = False
            RM_S3_PART_FAMILY_NAMES = ["RAINIER"]
            RMStudioFactory.clear()
            cls.factory = RMStudioFactory(
                RM_Factory,
                RM_ALL_PART_FAMILY_NAMES,
                RM_S1_PART_FAMILY_NAMES,
                RM_S2_PART_FAMILY_NAMES,
                RM_S3_PART_FAMILY_NAMES
            )
        return cls.factory
