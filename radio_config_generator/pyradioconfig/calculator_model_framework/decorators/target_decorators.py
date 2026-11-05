def skip_target_calc(method):
    def wrapper(self, model):
        if not hasattr(model.profile, 'skip_target_calculation'):
            raise AttributeError("Expected attribute 'skip_target_calculation' not found in model.profile")

        if model.profile.skip_target_calculation:
            return
        return method(self, model)

    return wrapper