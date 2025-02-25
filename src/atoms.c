#include <assert.h>
#include "atoms.h"
#include "atomList.h"
#include "errors.h"
#include "display.h"

AtomStruct* atomStorageStart = NULL;
static Atom lastUsedAtom = _NET_LAST_PREDEFINED;
AtomStruct preDefAtomStructResult;

AtomStruct* getAtomStruct(Atom atom) {
    AtomStruct* atomStruct = atomStorageStart;
    while (atomStruct != NULL) {
        if (atomStruct->atom == atom) { return atomStruct; }
        atomStruct = atomStruct->next;
    }
    return NULL;
}

AtomStruct* getAtomStructByName(const char* name) {
    int i;
    for (i = 0; i < PREDEFINED_ATOM_LIST_SIZE; i++) {
        if (strcmp(PredefinedAtomList[i].name, name) == 0) {
            preDefAtomStructResult.atom = PredefinedAtomList[i].atom;
            preDefAtomStructResult.name = PredefinedAtomList[i].name;
            return &preDefAtomStructResult;
        }
    }
    AtomStruct* atomStruct = atomStorageStart;
    while (atomStruct != NULL) {
        if (strcmp(atomStruct->name, name) == 0) { return atomStruct; }
        atomStruct = atomStruct->next;
    }
    return NULL;
}

Bool isValidAtom(Atom atom) {
    if (atom <= _NET_LAST_PREDEFINED) { return True; }
    return getAtomStruct(atom) != NULL;
}

const char* getAtomName(Display* display, Atom atom) {
    int i;
    for (i = 0; i < PREDEFINED_ATOM_LIST_SIZE; i++) {
        if (PredefinedAtomList[i].atom == atom) {
            const char* atomName = PredefinedAtomList[i].name;
            if (strncmp(atomName, "XA_", 3) == 0) {
                return &atomName[3];
            } else if (strncmp(atomName, "NET_", 4) == 0) {
                return &atomName[4];
            } else {
                return atomName;
            }
        }
    }
    return XGetAtomName(display, atom);
}

void freeAtomStorage() {
    AtomStruct* atomStorage;
    while ((atomStorage = atomStorageStart) != NULL) {
        atomStorageStart = atomStorage->next;
        free((char *) atomStorage->name);
        free(atomStorage);
    }
}

char* XGetAtomName(Display* display, Atom atom) {
    // https://tronche.com/gui/x/xlib/window-information/XGetAtomName.html
    SET_X_SERVER_REQUEST(display, X_GetAtomName);
    const char* atomName = getAtomName(display, atom);
    if (atomName == NULL) {
        handleError(0, display, None, 0, BadAtom, 0);
        return NULL;
    }
    return (char*)atomName; // cast to match the return type
}

Status XGetAtomNames(Display *dpy, Atom *atoms, int count, char **names_return) {
    // https://tronche.com/gui/x/xlib/window-information/XGetAtomNames.html
    // This function returns a nonzero status if names are returned for all of the atoms; otherwise, it returns zero.
    int returned_names = 0;
    for (int i = 0; i < count; ++i) {
        char *name = XGetAtomName(dpy, atoms[i]);
        if (name != NULL) returned_names++;
        names_return[i] = name;
    }
    return returned_names == count ? 1 : 0;
}

Atom _internAtom(const char* atomName, Bool only_if_exists, Bool* outOfMemory) {
    if (outOfMemory != NULL) *outOfMemory = False;
    AtomStruct* atomStruct = getAtomStructByName(atomName);
    if (atomStruct != NULL) {
        return atomStruct->atom;
    } else if (!only_if_exists) {
        atomStruct = malloc(sizeof(AtomStruct));
        if (atomStruct == NULL) {
            if (outOfMemory != NULL) *outOfMemory = True;
            return None;
        }
        atomStruct->name = strdup(atomName);
        if (atomStruct->name == NULL) {
            free(atomStruct);
            if (outOfMemory != NULL) *outOfMemory = True;
            return None;
        }
        atomStruct->atom = ++lastUsedAtom;
        atomStruct->next = atomStorageStart;
        atomStorageStart = atomStruct;
        return atomStruct->atom;
    }
    return None;
}

Atom internalInternAtom(const char* atomName) {
    return _internAtom(atomName, False, NULL);
}

Atom XInternAtom(Display* display, _Xconst char* atom_name, Bool only_if_exists) {
    // https://tronche.com/gui/x/xlib/window-information/XInternAtom.html
    SET_X_SERVER_REQUEST(display, X_InternAtom);
    int preExistingIndex = -1;
    if (strncmp(atom_name, "XA_", 3) == 0) {
        int i = 0;
        do {
            if (strcmp(&PredefinedAtomList[i].name[4], &atom_name[4]) == 0) {
                preExistingIndex = i;
                break;
            }
        } while (PredefinedAtomList[i++].atom != XA_WM_TRANSIENT_FOR);
    } else if (strncmp(atom_name, "NET_", 4) == 0) {
        int i = 78;
        assert(PredefinedAtomList[i].atom == _NET_WM_NAME);
        do {
            if (strcmp(&PredefinedAtomList[i].name[5], &atom_name[5]) == 0) {
                preExistingIndex = i;
                break;
            }
        } while (PredefinedAtomList[i++].atom != _NET_FRAME_EXTENTS);
    }
    if (preExistingIndex >= 0) {
        return PredefinedAtomList[preExistingIndex].atom;
    }
    Bool outOfMemory;
    Atom result = _internAtom(atom_name, only_if_exists, &outOfMemory);
    if (outOfMemory) {
        handleOutOfMemory(0, display, 0, 0);
    }
    return result;
}

Status XInternAtoms (Display *dpy, char **names, int count, Bool onlyIfExists, Atom *atoms_return) {
    // This function returns a nonzero status if atoms are returned for all of the names; otherwise, it returns zero.
    int returned_atoms = 0;
    for (int i = 0; i < count; ++i) {
        Atom atom = XInternAtom(dpy, names[i], onlyIfExists);
        if (atom != None) returned_atoms++;
        atoms_return[i] = atom;
    }
    return returned_atoms == count ? 1 : 0;
}
