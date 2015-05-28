param(
    [String]$pathToExecutable='',
    [String]$username='runner',
    [String]$password='12345',
    [String]$dir='.',
    [String[]]$canRead=@(),
    [String[]]$canWrite=@()
)

#check for zero length path to spawner
$pathToExecutable = [String](rvpa $pathToExecutable)

function EnsureObject([String]$path, [String]$type) {
    New-Item -Path $Path -ItemType $Type -Force
}

function IsDirectory([String]$Path) {
    if ((Get-Item $path) -is [System.IO.DirectoryInfo]) {
        return $true
    } else {
        return $false
    }
}

function AddAccessRules(
    [String]$UserName,
    [String[]]$Rights,
    [System.Security.AccessControl.AccessControlType]$AccessControlType,
    [System.Security.AccessControl.InheritanceFlags[]]$InheritanceFlags
) {
    $user = New-Object System.Security.Principal.NTAccount($UserName)
    
    $colRights = [System.Security.AccessControl.FileSystemRights]($Rights -join ', ')
    
    $accessRule = New-Object System.Security.AccessControl.FileSystemAccessRule `
    (
        $user,
        $colRights,
        [System.Security.AccessControl.InheritanceFlags]::None,
        [System.Security.AccessControl.PropagationFlags]::None,
        $AccessControlType
    )
    
    $acl = Get-ACL -path $path
    
    if ($acl) { 
        $acl.AddAccessRule($accessRule)
    
        Set-ACL -Path $path -AclObject $acl
    }
}

function Allow(
    [String]$UserName,
    [String[]]$Rights,
    [String]$Path,
    [System.Security.AccessControl.InheritanceFlags[]]$InheritanceFlags = [System.Security.AccessControl.InheritanceFlags]::None
) {
    AddAccessRules -username $UserName -rights $Rights -path $Path -accessControlType ([System.Security.AccessControl.AccessControlType]::Allow) -InheritanceFlags $inheritanceFlags
}

function Deny(
    [String]$UserName,
    [String[]]$Rights,
    [String]$Path,
    [System.Security.AccessControl.InheritanceFlags[]]$InheritanceFlags = [System.Security.AccessControl.InheritanceFlags]::None
) {
    AddAccessRules -username $UserName -rights $Rights -path $Path -accessControlType ([System.Security.AccessControl.AccessControlType]::Deny) -InheritanceFlags $inheritanceFlags
}


# set up user

$Computer = [ADSI]"WinNT://$Env:COMPUTERNAME,Computer"

try
{
    $computer.delete('user', $username);
}
catch
{}

$runner = $Computer.Create("User", $username)
$runner.SetPassword($password)
$runner.SetInfo()
$runner.UserFlags = 64 + 65536 # ADS_UF_PASSWD_CANT_CHANGE + ADS_UF_DONT_EXPIRE_PASSWD
$runner.SetInfo()


# set up working directory and requested files

EnsureObject -Path $dir -Type 'dir'
Allow -UserName $username -Rights 'Read' -Path $dir
Deny -UserName $username -Rights 'Write', 'TakeOwnership', 'ChangePermissions', 'ReadPermissions', 'DeleteSubdirectoriesAndFiles', 'Delete' -Path $dir

foreach ($file in $canRead) {
    $path = $dir + '\' + $file

    EnsureObject -Path $path -Type 'file'
    Allow -UserName $username -Rights 'Read' -Path $path
    Deny -UserName $username -Rights 'Delete', 'Write', 'ChangePermissions', 'TakeOwnership' -Path $path
}

foreach ($file in $canWrite) {
    $path = $dir + '\' + $file

    EnsureObject -Path $path -Type 'file'
    Allow -UserName $username -Rights 'Write' -Path $path
    Deny -UserName $username -Rights 'Delete', 'ChangePermissions', 'TakeOwnership' -Path $path
}


#deny access to registry

$registryVolumes = @(gdr -PSProvider 'Registry' | Select -ExpandProperty Name)

foreach ($volume in $registryVolumes) {
    $path = $volume + ':\'
    
    $person = New-Object System.Security.Principal.NTAccount($username)
    $access = @('SetValue', 'CreateSubKey', 'CreateLink', 'WriteKey', 'ChangePermissions', 'TakeOwnership', 'Delete', 'Notify', 'CreateLink')
    $inheritance = [System.Security.AccessControl.InheritanceFlags]::None
    $propagation = [System.Security.AccessControl.PropagationFlags]::None
    $type = [System.Security.AccessControl.AccessControlType]::Deny

    $rule = New-Object System.Security.AccessControl.RegistryAccessRule($person, $access, $inheritance, $propagation, $type)
    
    $acl = Get-Acl $path
    
    $acl.AddAccessRule($rule)
    
    Set-ACL -Path $path -AclObject $acl
}

#deny access to filesystem around working directory

$wd = [String](rvpa ($dir + "\.."))
$from = [String](rvpa $dir)
$inheritedByAllChilds = @([System.Security.AccessControl.InheritanceFlags]::ObjectInherit, [System.Security.AccessControl.InheritanceFlags]::ContainerInherit)
$whatDenied = @('FullControl')


function RecurseDeny([String]$Username, [String]$path, [String[]]$whatDenied) {
    if ($path -eq $pathToExecutable) {
        Deny -UserName $userName -Rights 'Write', 'Delete', 'ChangePermissions', 'TakeOwnership' -Path $path
    } else {
        Deny -UserName $Username -Rights $whatDenied -Path $path
    }
    
    if (isDirectory -Path $path) {
        $children = @(Get-ChildItem -Path $path | Select -ExpandProperty Name)
    
        if ($children.length -ne 0) { #ugly and strange but required
            foreach ($child in $children) {
                RecurseDeny -Username $Username -Path ($path + "\" + $child) -whatDenied $whatDenied
            }
        }        
    }    
}

while ($wd -ne $from) {
    $fromDir = Split-Path $from -Leaf
    $siblings = Get-ChildItem $wd | Select -ExpandProperty Name | where { $_ -ne $fromDir }
    
    foreach ($sibling in $siblings) {
        RecurseDeny -UserName $username -WhatDenied $whatDenied -Path ($wd + '\' + $sibling)
    }

    Deny -UserName $username -Rights $whatDenied -Path $wd

    $from = $wd
    $wd = [String](rvpa ($wd + "\.."))
}
