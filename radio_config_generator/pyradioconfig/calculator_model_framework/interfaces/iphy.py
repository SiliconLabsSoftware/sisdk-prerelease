from abc import ABCMeta, abstractmethod
import inspect
import sys
import types
from pycalcmodel.core.phy import ModelPhy
import os

"""
Phy interface file

This has been updated to handle 3 scenarios:
1) makePhy can be called inside of PHY_PHY1Example, and then if another PHY wishes to call PHY1Example the phy_name and phy_group_name will actually be set based on the highest level PHY in the call stack.
2) makePhy can be called inside of PHY_PHY1Example, and then if another PHY wishes to call PHY1Example they can optionally pass in the phy_name and phy_group_name as parameters.
3) makePhy can be called inside of a high level PHY_PHY2Example that calls PHY1Example. If makePhy is also called inside PHY1Example, then the lower level call will actually not execute and instead return the phy for PHY2Example.

This allows a lot of flexibility in how PHYs are created, and should make PHYs "just work" in most cases.
"""
from pycalcmodel.core.common import IPCategory
from pyradioconfig.calculator_model_framework.mixins.xtal_frequency import XtalFrequencyMixin

class IPhy(XtalFrequencyMixin):

    ip_category = IPCategory.LPW       # Default category

    """
    Returns list of callable phy functions
    """
    @abstractmethod
    def getPhyList(self):
        functionList = list()
        for functionName, functionPointer in inspect.getmembers(self, predicate=inspect.ismethod):
                if str(functionName).lower().startswith('phy_'):  # Check if the function name starts with "phy_"
                    if isinstance(functionPointer, types.MethodType):
                        functionList.append(functionPointer)
        return functionList

    """
    Builds empty phy model
    """
    @abstractmethod
    def _makePhy(self, modem_model, profile, phy_description=None, phy_group_name=None, readable_name=None, act_logic='', tags='', phy_name=None, locked=False):
        assert self.ip_category == profile.ip_category, "Error: IP category of PHY does not match IP category of profile, check that the profile assigned to this PHY has the corresponding IP category"

        existing_phy = self.__get_phy_already_created(modem_model)
        if existing_phy == None:
            if phy_name is None:
                phy_name = self.__highest_caller_phy()
            if phy_description is None:
                if readable_name is None:
                    phy_description = phy_name
                else:
                    phy_description = readable_name
            if phy_group_name is None:
                phy_group_name = self.__highest_caller_group()
            if readable_name is None:
                readable_name = phy_name.replace("_", " ")
            if tags is None:
                tags = ""
            if '_prod' in phy_name.lower():
                phy_points_to = self.__get_points_to_phy()
            else:
                phy_points_to = None

            if hasattr(self, 'phy_guids') and (modem_model.part_family, phy_name) in self.phy_guids:
                guid = self.phy_guids[modem_model.part_family, phy_name]
            else:
                guid = None

            phy = ModelPhy(phy_name, phy_description, profile, phy_group_name, readable_name=readable_name, act_logic=act_logic, tags=tags, phy_points_to = phy_points_to, locked=locked, guid=guid, ip_category=self.ip_category)
            try:
                duplicate_phy = getattr(modem_model, phy_name)
            except AttributeError:
                modem_model.phys.append(phy)
            else:
                raise Exception("Error: duplicate definition found for %s" %phy_name)

            return phy
        else:
            return existing_phy

    """
    Helper methods
    """

    def __highest_caller_phy(self):
        frame = sys._getframe()
        phy_callers = []
        try:
            while frame is not None:
                caller_name = frame.f_code.co_name
                if caller_name.startswith("PHY_"):
                    phy_callers.append(caller_name)
                frame = frame.f_back
        except:
            pass
        
        if phy_callers:
            return phy_callers[-1]  #Return the highest level caller with PHY_ in its name

        raise AssertionError('Did not find a PHY method to name the PHY, check that the method is named correctly')

    def __highest_caller_group(self):
        frame = sys._getframe()
        phy_file_path = None
        try:
            while frame is not None:
                caller_name = frame.f_code.co_name
                if caller_name.startswith("PHY_"):
                    phy_file_path = frame.f_code.co_filename
                frame = frame.f_back
        except:
            pass
        
        if phy_file_path:
            file_name = os.path.basename(phy_file_path)
            file_name_no_ext = os.path.splitext(file_name)[0]
            return file_name_no_ext #Return the file name of the highest level caller with PHY_ in its name

        raise AssertionError('Did not find a PHY method to name the PHY group, check that the method is named correctly')

    def __get_phy_already_created(self, model):


        stack_frame = inspect.currentframe()
        last_frame = None
        lowest_phy_frame = None
        highest_phy_frame = None

        while lowest_phy_frame is None or highest_phy_frame is None:
            frame_name = stack_frame.f_code.co_name
            frame_file = stack_frame.f_code.co_filename
            is_decorator_frame = frame_name == "wrapped" and "phy_decorators" in frame_file
            if frame_name.startswith("PHY_"):
                if lowest_phy_frame is None:
                    lowest_phy_frame = stack_frame
            elif not is_decorator_frame:
                if lowest_phy_frame is not None:
                    highest_phy_frame = last_frame
            try:
                last_frame = stack_frame
                stack_frame = stack_frame.f_back
            except AttributeError:
                break

        if lowest_phy_frame is None:
            raise Exception('Did not find a PHY method to name the PHY, check that the method is named correctly')
        elif lowest_phy_frame == highest_phy_frame:
            return None
        else:
            try:
                highest_phy_name = highest_phy_frame.f_code.co_name
                highest_phy = getattr(model.phys, highest_phy_name)
            except AttributeError:
                return None
            else:
                return highest_phy

    def __get_points_to_phy(self):
        frame = sys._getframe()
        phy_callers = []
        try:
            while frame is not None:
                caller_name = frame.f_code.co_name
                if caller_name.startswith("PHY_"):
                    phy_callers.append(caller_name)
                frame = frame.f_back
        except:
            pass
        
        found_prod_phy = False
        for caller_name in reversed(phy_callers):
            if "_prod" in caller_name.lower():
                found_prod_phy = True
            elif found_prod_phy:
                return caller_name

        return None
