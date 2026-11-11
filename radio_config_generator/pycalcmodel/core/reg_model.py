from pycalcmodel.core.rm_studio_factory import RMStudioFactory

class RegModel:

    _reg_model_dict = dict()

    @classmethod
    def get_reg_model(cls, part_family: str, part_revision: str | None = None):

        if not RMStudioFactory.is_exist():
            raise RuntimeError("RMStudioFactory instance has not been initialized.")

        rm_studio = RMStudioFactory.get_instance()

        part_family = part_family.lower()

        if part_family in cls._reg_model_dict.keys():
            reg_model = cls._reg_model_dict[part_family]
        else:
            if part_family.upper() in rm_studio.RM_ALL_PART_FAMILY_NAMES:
                if (part_revision == 'ANY'
                        or part_family.upper() in rm_studio.RM_S1_PART_FAMILY_NAMES
                        or part_family.upper() in rm_studio.RM_S2_PART_FAMILY_NAMES
                        or part_family.upper() in rm_studio.RM_S3_PART_FAMILY_NAMES):
                    cls._reg_model_dict[part_family] = rm_studio.RM_Factory(part_family.upper())()
                else:
                    cls._reg_model_dict[part_family] = rm_studio.RM_Factory(part_family.upper(), part_revision)()
                reg_model = cls._reg_model_dict[part_family]
            else:
                reg_model = None

        return reg_model
