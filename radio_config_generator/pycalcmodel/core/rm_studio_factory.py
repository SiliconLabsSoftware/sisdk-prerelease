from __future__ import annotations
from dataclasses import dataclass, field
from typing import Protocol, Any, ClassVar
import warnings

__all__ = ["RMStudioFactory"]

class RMFactoryCallable(Protocol):
    def __call__(
        self,
        part_family_name: str,
        rev_name: str | None = None
    ) -> type[Any]:
        ...

class DefaultDevice:
    pass

def default_rm_factory(
    part_family_name: str,
    rev_name: str | None = None,
) -> type[DefaultDevice]:
    return DefaultDevice

@dataclass
class RMStudioFactory:
    """
    Create an extra helper class (dataclass) for managing host_py_rm_studio_internal.factory values
    This is a singleton that stores the reg model only for parts accessed during execution
    """

    RM_Factory: RMFactoryCallable = default_rm_factory
    RM_ALL_PART_FAMILY_NAMES: list[str] = field(default_factory=list)
    RM_S1_PART_FAMILY_NAMES: list[str] = field(default_factory=list)
    RM_S2_PART_FAMILY_NAMES: list[str] = field(default_factory=list)
    RM_S3_PART_FAMILY_NAMES: list[str] = field(default_factory=list)

    _instance: ClassVar[RMStudioFactory  | None] = None
    _initialized: bool = field(default=False, init=False, repr=False, compare=False)

    def __new__(cls, *args, **kwargs) -> RMStudioFactory:
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def __init__(
        self,
        RM_Factory: RMFactoryCallable = default_rm_factory,
        RM_ALL_PART_FAMILY_NAMES: list[str] | None = None,
        RM_S1_PART_FAMILY_NAMES: list[str] | None = None,
        RM_S2_PART_FAMILY_NAMES: list[str] | None = None,
        RM_S3_PART_FAMILY_NAMES: list[str] | None = None,
    ) -> None:
        if self._initialized:
            warnings.warn("RMStudioFactory instance is already initialized.", UserWarning)
            return

        self.RM_Factory = RM_Factory
        self.RM_ALL_PART_FAMILY_NAMES = (RM_ALL_PART_FAMILY_NAMES if RM_ALL_PART_FAMILY_NAMES is not None else [])
        self.RM_S1_PART_FAMILY_NAMES = (RM_S1_PART_FAMILY_NAMES if RM_S1_PART_FAMILY_NAMES is not None else [])
        self.RM_S2_PART_FAMILY_NAMES = (RM_S2_PART_FAMILY_NAMES if RM_S2_PART_FAMILY_NAMES is not None else [])
        self.RM_S3_PART_FAMILY_NAMES = (RM_S3_PART_FAMILY_NAMES if RM_S3_PART_FAMILY_NAMES is not None else [])
        self._initialized = True

    @classmethod
    def get_instance(cls) -> RMStudioFactory:
        if cls._instance is None or not cls._instance._initialized:
            raise RuntimeError("RMStudioFactory instance has not been initialized.")
        return cls._instance

    @classmethod
    def is_exist(cls) -> bool:
        return cls._instance is not None and cls._instance._initialized

    @classmethod
    def clear(cls) -> None:
        if cls._instance is None:
            return

        cls._instance.RM_Factory = default_rm_factory
        cls._instance.RM_ALL_PART_FAMILY_NAMES = []
        cls._instance.RM_S1_PART_FAMILY_NAMES = []
        cls._instance.RM_S2_PART_FAMILY_NAMES = []
        cls._instance.RM_S3_PART_FAMILY_NAMES = []
        cls._instance._initialized = False
