class Source_Code(object):

    @staticmethod
    def print_modem_model_forced_as_source(outputfilename, phy_name, modem_model):

        output_lines = list()

        line = 'def %s(modem_model): ' % phy_name
        output_lines.append(line)

        for var in modem_model.vars:
            if var.svd_mapping:
                if var._value_forced is not None:       # gdc:  What's the right way to check this?
                    if var.var_enum is not None:
                        line = "    phy.profile_inputs.%s.value = model.vars.%s.var_enum.%s" % (
                        var.name, var.name, str(var.value).split(".")[1])
                    else:
                        line = "    phy.profile_inputs.%s.value = %r" % (var.name, var.value_forced)
                    output_lines.append(line)
            else:
                if var._value_forced is not None:
                    if var.var_enum is not None:
                        line = "    phy.profile_inputs.%s.value = model.vars.%s.var_enum.%s" % (
                        var.name, var.name, str(var.value).split(".")[1])
                    else:
                        line = "    phy.profile_inputs.%s.value = %r" % (var.name, var.value_forced)
                    output_lines.append(line)


        outputfile = open(outputfilename, 'w')
        outputfile.write('\n'.join(output_lines) + '\n\n')
        outputfile.close()