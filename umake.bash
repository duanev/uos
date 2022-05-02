#/usr/bin/env bash
export UOS_GIT_DIR=$PWD

_umake_completions()
{
    local ln=( $COMP_LINE )
    if [ "$1" == "$3" ]; then
        (cd $UOS_GIT_DIR/arch; printf "%s \n" "$2"*)
    else
        case ${#ln[@]} in
        2)
            (cd $UOS_GIT_DIR/arch/$3/mach; printf "%s \n" "$2"*)
            ;;
        3)
            if [ -d "$UOS_GIT_DIR/arch/${ln[1]}/mach/$3" ]; then
                (cd $UOS_GIT_DIR/apps; printf "%s \n" "$2"*)
            else
                # machine type was not fully specified
                (cd $UOS_GIT_DIR/arch/${ln[1]}/mach; printf "%s \n" "$2"*)
            fi
            ;;
        4)
            (cd $UOS_GIT_DIR/apps; printf "%s \n" "$2"*)
            ;;
        esac
    fi
} && complete -o nospace -C _umake_completions umake

# complete -F _umake_completions umake
